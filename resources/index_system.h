#ifndef TENSOR_INDEX_SYSTEM_H
#define TENSOR_INDEX_SYSTEM_H


namespace tensor { namespace index_system {
    struct strategy_type
    {
      enum _enum
      {
	constant = 0,
	monotonically_increasing,
	monotonically_decreasing,
	indexed,
      };
    };

    template<enum strategy_type::_enum>
    struct strategy
    {
    };

    template<>
    struct strategy<strategy_type::constant>
    {
      static strategy_type::_enum strategy_type() { return strategy_type::constant; }
      int m_constant;
      __device__ strategy( int c ) : m_constant(c) {}
      __device__ int operator()(int) const { return m_constant; }
    };

    template<>
    struct strategy<strategy_type::monotonically_increasing>
    {
      static strategy_type::_enum strategy_type() { return strategy_type::monotonically_increasing; }
      int m_length;
      __device__ strategy( int l ) : m_length(l){}
      __device__ int operator()(int elem_idx) const {
	return elem_idx % m_length;
      }
    };

    template<>
    struct strategy<strategy_type::monotonically_decreasing>
    {
      static strategy_type::_enum strategy_type() { return strategy_type::monotonically_decreasing; }
      int m_length;
      __device__ strategy( int l ) : m_length(l){}
      __device__ int operator() (int elem_idx) const {
	return m_length - (elem_idx % m_length) - 1;
      }
    };


    template<>
    struct strategy<strategy_type::indexed>
    {
      static strategy_type::_enum strategy_type() { return strategy_type::indexed; }
      const int* m_indexes;
      int m_index_length;
      __device__ strategy( const int* idxes, int idx_len )
	: m_indexes( idxes ), m_index_length(idx_len) {}
      __device__ int operator()(int elem_idx) const {
	return m_indexes[(elem_idx % m_index_length)];
      }
    };

    struct passthrough
    {
      __device__ int operator()(int arg) const { return arg; }
    };

    struct addr_layout
    {
      int num_columns;
      int column_stride;
      __device__ addr_layout( int nc, int cs )
	: num_columns(nc), column_stride(cs) {
      }
      __device__ int operator()(int idx) const {
	return idx % num_columns +
	  (column_stride * (idx / num_columns));
      }
    };

    template<typename strat_type_t, typename addr_layout_t>
    struct specific_index_system
    {
      strat_type_t m_strategy;
      addr_layout_t m_addrlayout;
      __device__ specific_index_system(strat_type_t st, addr_layout_t ad)
	: m_strategy(st)
	, m_addrlayout(ad) {
      }
      __device__ int operator()(int idx) const {
	return m_addrlayout(m_strategy(idx));
      }
    };

    struct general_index_system
    {
      int type;
      int c_or_len;
      const int* indexes;
      int num_columns;
      int column_stride;

      __host__ __device__ general_index_system(int t, int c_l, const int* idx,
					       int n_c, int col_s)
	: type(t)
	, c_or_len(c_l)
	, indexes(idx)
	, num_columns(n_c)
	, column_stride(col_s) {
      }
    };


    template<typename operator_t>
    __device__
    void with_index_strategy( const general_index_system& sys, operator_t op)
    {
      switch(sys.type)
      {
      case strategy_type::constant:
	op(strategy<strategy_type::constant>(sys.c_or_len));
	break;
      case strategy_type::monotonically_increasing:
	op(strategy<strategy_type::monotonically_increasing>(sys.c_or_len));
	break;
      case strategy_type::monotonically_decreasing:
	op(strategy<strategy_type::monotonically_decreasing>(sys.c_or_len));
	break;
      case strategy_type::indexed:
	op(strategy<strategy_type::indexed>(sys.indexes, sys.c_or_len));
	break;
      };
    }

    __device__
    int get_index_from_strategy( const general_index_system& sys, int elem)
    {
      switch(sys.type)
      {
      case strategy_type::constant:
	return strategy<strategy_type::constant>(sys.c_or_len)(elem);
      case strategy_type::monotonically_increasing:
	return strategy<strategy_type::monotonically_increasing>(sys.c_or_len)(elem);
      case strategy_type::monotonically_decreasing:
	return strategy<strategy_type::monotonically_decreasing>(sys.c_or_len)(elem);
      case strategy_type::indexed:
	return strategy<strategy_type::indexed>(sys.indexes, sys.c_or_len)(elem);
      };
      return 0;
    }

    template<typename operator_t>
    __device__
    void with_addr_layout(const general_index_system& sys, operator_t op)
    {
      if (sys.num_columns == sys.column_stride) {
	op(passthrough());
      }
      else {
	op(addr_layout(sys.num_columns, sys.column_stride));
      }
    }

    __device__
    int get_index_with_layout(const general_index_system& sys, int idx)
    {
      if (sys.num_columns == sys.column_stride) {
	return idx;
      }
      else {
	return addr_layout(sys.num_columns, sys.column_stride)(idx);
      }
    }

    template<typename operator_t, typename strategy_t>
    struct layout_combiner
    {
      operator_t m_op;
      strategy_t m_strat;
      __device__ layout_combiner(operator_t op, strategy_t strat)
	: m_op( op)
	, m_strat(strat) {
      }
      template<typename addr_layout_t>
      __device__ void operator()(addr_layout_t layout) {
	m_op(specific_index_system<strategy_t,addr_layout_t>(m_strat, layout));
      }
    };

    template<typename operator_t>
    struct strategy_combiner
    {
      operator_t m_op;
      const general_index_system& m_sys;
      __device__ strategy_combiner(operator_t op, const general_index_system& sys)
	: m_op( op)
	, m_sys( sys) {
      }
      template<typename strategy_t>
      __device__ void operator()(strategy_t strategy) {
	with_addr_layout(m_sys, layout_combiner<operator_t, strategy_t>(m_op, strategy));
      }
    };

    template<typename operator_t>
    __device__
    void with_index_system( const general_index_system& sys, operator_t op)
    {
      with_index_strategy(sys, strategy_combiner<operator_t>(op, sys));
    }

    template<typename operator_t, typename lhs_strat_type>
    struct index_2_rhs_sys
    {
      operator_t m_op;
      lhs_strat_type m_lhs_sys;
      __device__ index_2_rhs_sys(lhs_strat_type lhs_sys, operator_t op )
	: m_op ( op )
	, m_lhs_sys( lhs_sys ) {
      }
      template<typename rhs_strat_type>
      void __device__ operator()( rhs_strat_type rhs_sys ) {
	m_op( m_lhs_sys, rhs_sys );
      }
    };

    template<typename operator_t>
    struct index_2_lhs_sys
    {
      operator_t m_op;
      const general_index_system& m_rhs;
      __device__ index_2_lhs_sys( const general_index_system& rhs,
				  operator_t op)
	: m_rhs( rhs )
	, m_op( op ){
      }
      template<typename strategy_type>
      __device__ void operator() (strategy_type lhs) {
	with_index_system( m_rhs, index_2_rhs_sys<operator_t, strategy_type>( lhs, m_op ) );
      }
    };


    template<typename operator_t>
    __device__
    void with_2_index_systems( const general_index_system& lhs_sys,
			       const general_index_system& rhs_sys,
			       operator_t op)
    {
      with_index_system(lhs_sys, index_2_lhs_sys<operator_t>(rhs_sys, op));
    }

    template<typename operator_t, typename dest_strat_type>
    struct index_3_binder
    {
      operator_t m_op;
      dest_strat_type m_dest_sys;
      __device__ index_3_binder( operator_t op,
				 dest_strat_type dest_sys )
	: m_op ( op )
	, m_dest_sys( dest_sys ) {
      }
      template<typename lhs_strat_type, typename rhs_strat_type>
      __device__ void operator()( lhs_strat_type lhs_sys, rhs_strat_type rhs_sys ) {
	m_op( m_dest_sys, lhs_sys, rhs_sys );
      }
    };

    template<typename op_t>
    struct index_3_op
    {
      op_t m_op;
      const general_index_system& lhs_sys;
      const general_index_system& rhs_sys;
      __device__ index_3_op( op_t op,
			     const general_index_system& _lhs_sys,
			     const general_index_system& _rhs_sys)
	: m_op (op)
	, lhs_sys(_lhs_sys)
	, rhs_sys(_rhs_sys){}
      template<typename strategy>
      __device__ void operator()(strategy dest) {
	with_2_index_systems(lhs_sys, rhs_sys,
			     index_3_binder<op_t, strategy>( m_op, dest ) );
      }
    };


    template<typename operator_t>
    __device__
    void with_3_index_systems( const general_index_system& dest_sys,
			       const general_index_system& lhs_sys,
			       const general_index_system& rhs_sys,
			       operator_t op)
    {
      with_index_system(dest_sys,
			index_3_op<operator_t>(op, lhs_sys, rhs_sys));
    }

  }}


#define EXPLODE_IDX_SYSTEM(varname)					\
  int type##varname, int c_or_len##varname, const int* ptr##varname,	\
    int num_cols##varname, int column_stride##varname

#define ENCAPSULATE_IDX_SYSTEM(varname)					\
  general_index_system(type##varname, c_or_len##varname, ptr##varname,	\
		       num_cols##varname, column_stride##varname)



#endif
