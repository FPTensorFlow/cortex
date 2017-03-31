(ns cortex.verify.tensor
  (:require [cortex.tensor :as ct]
            [cortex.compute.cpu.driver :as cpu-driver]
            [cortex.compute.driver :as drv]
            [clojure.test :refer :all]
            [clojure.core.matrix :as m]))


(defmacro tensor-context
  [driver datatype & body]
  `(with-bindings {#'ct/*stream* (drv/create-stream ~driver)
                   #'ct/*datatype* ~datatype}
     ~@body))


(defn assign-constant!
  [driver datatype]
  (tensor-context
   driver datatype
   (let [tensor (ct/->tensor (partition 3 (range 9)))]
     (is (= (ct/ecount tensor) 9))
     (is (m/equals (range 9)
                   (ct/to-double-array tensor)))
     (ct/assign! tensor 1)
     (is (m/equals (repeat 9 1)
                   (ct/to-double-array tensor)))

     (let [rows (ct/rows tensor)
           columns (ct/columns tensor)]
       (doseq [row rows]
         (ct/assign! row 2))
       (is (m/equals (repeat 9 2)
                     (ct/to-double-array tensor)))
       (let [[c1 c2 c3] columns]
         (ct/assign! c1 1)
         (ct/assign! c2 2)
         (ct/assign! c3 3))
       (is (m/equals (flatten (repeat 3 [1 2 3]))
                     (ct/to-double-array tensor)))))))


(defn assign-marshal
  "Assignment must be capable of marshalling data.  This is an somewhat difficult challenge
for the cuda backend."
  [driver datatype]
  (tensor-context
   driver datatype
   (let [tensor (ct/->tensor (partition 3 (range 9)))
         intermediate (ct/new-tensor [3 3] :datatype :int)
         final (ct/new-tensor [3 3] :datatype :double)]
     (ct/assign! intermediate tensor)
     (ct/assign! final intermediate)
     (is (m/equals (range 9)
                   (ct/to-double-array final))))))


(defn binary-constant-op
  [driver datatype]
  (tensor-context
   driver datatype
   (let [tens-a (ct/->tensor (partition 3 (range 9)))
         tens-b (ct/->tensor (repeat 9 1))]

     (when-not (= datatype :byte)
       (ct/binary-op! tens-b 2.0 tens-a 3.0 4.0 :*)
       (is (m/equals (mapv #(* 24 %) (range 9))
                     (ct/to-double-array tens-b))))

     (ct/binary-op! tens-b 2.0 tens-a 3.0 4.0 :+)
     (is (m/equals (mapv #(+ 12 (* 2 %)) (range 9))
                   (ct/to-double-array tens-b)))


     ;;Check reversing operands works.
     (ct/binary-op! tens-b 3.0 4.0 2.0 tens-a :-)
     (is (m/equals (mapv #(- 12 (* 2 %)) (range 9))
                   (ct/to-double-array tens-b)))

     (ct/assign! tens-b 1.0)
     (is (m/equals (repeat 9 1)
                   (ct/to-double-array tens-b)))

     ;;Check accumulation
     (ct/binary-op! tens-b 1.0 tens-b 1.0 1.0 :+)
     (is (m/equals (mapv #(+ 1 %) (repeat 9 1))
                   (ct/to-double-array tens-b))))))
