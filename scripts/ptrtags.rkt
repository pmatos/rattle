#lang rosette/safe

(current-bitwidth #f)
(define bw 32) (define ptrmask #x7)
;(define bw 64) (define ptrmask #xf)

(define native_t (bitvector bw))

(define-symbolic ptr native_t)

(define-symbolic null-constant native_t)

(define-symbolic bool-value boolean?)
(define-symbolic true-constant native_t)
(define-symbolic false-constant native_t)

(define-symbolic fx-value integer?)

(define-symbolic char-value integer?)

(define (ptr? v)
  (bveq (bvand v (bv ptrmask bw))
        (bv 0 (bitvector bw))))

(define-symbolic fx-tag native_t)
(define-symbolic fx-shift native_t)
(define-symbolic fx-mask native_t)

(define (fx? v)
  (bveq (bvand v fx-mask) fx-tag))

(define (null? v)
  (bveq null-constant v))

(define-symbolic char-tag native_t)
(define-symbolic char-shift native_t)
(define-symbolic char-mask native_t)

(define (char? v)
  (bveq (bvand v char-mask) char-tag))

(define-symbolic bool-tag native_t)
(define-symbolic bool-shift native_t)
(define-symbolic bool-mask native_t)

(define (bool? v)
  (bveq (bvand v bool-mask) bool-tag))

(define (valid? v)
  (= 1
     (+ (if (ptr? v) 1 0)
        (if (char? v) 1 0)
        (if (fx? v) 1 0)
        (if (bool? v) 1 0)
        (if (null? v) 1 0))))

;; Encoding procedures
(define (fx-encode i)
  (bvor fx-tag (bvshl (integer->bitvector i native_t) fx-shift)))

(define (char-encode c)
  (bvor char-tag (bvshl (integer->bitvector c native_t) char-shift)))

(define (bool-encode b)
  (if b
      (bvor bool-tag (bvshl (bv 1 bw) bool-shift))
      (bvor bool-tag (bvshl (bv 0 bw) bool-shift))))

(assert (bveq true-constant (bool-encode #t)))
(assert (bveq false-constant (bool-encode #f)))

;; Decoding procedures
(define (bool-decode b)
  (bveq (bvashr b bool-shift) (bv 1 bw)))

(define (char-decode c)
  (bvashr c char-shift))

(define (fx-decode fx)
  (bvashr fx fx-shift))

(synthesize
 #:forall (list bool-value fx-value char-value ptr)
 #:guarantee
 (begin
   
   ;; bool constraints
   (assert (and (valid? (bool-encode bool-value))
                (bool? (bool-encode bool-value))
                (<=> bool-value (bool-decode (bool-encode bool-value)))))

   ;; null constraints
   (assert (valid? null-constant) (null? null-constant))
   
   ;; fx constraints
   (assert (=> (>= (- (expt 2 (- bw 1)))
                   fx-value
                   (- (expt 2 (- bw 1)) 1))
               (and (valid? (fx-encode fx-value))
                    (fx? (fx-encode fx-value))
                    (= fx-value (fx-decode (fx-encode fx-value))))))

   ;; ptr constraints (non-imm)
   (assert (=> (ptr? ptr) (valid? ptr)))

   ;; char constraints
   (assert (=> (>= 0 char-value 255)
               (and (valid? (char-encode char-value))
                    (char? (char-encode char-value))
                    (= char-value (char-decode (char-encode char-value))))))))