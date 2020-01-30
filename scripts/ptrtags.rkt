#lang rosette/safe

(current-bitwidth #f)
(define bw 32) (define ptrmask #x7)
;(define bw 64) (define ptrmask #xf)

(define native_t (bitvector bw))

(define-symbolic null-value1 native_t)
(define-symbolic null-value2 native_t)

(define-symbolic fx-value native_t)

(define (ptr? v)
  (bveq (bvand v (bv ptrmask bw))
        (bv 0 (bitvector bw))))

(define-symbolic fx-tag native_t)
(define-symbolic fx-shift native_t)
(define-symbolic fx-mask native_t)

(define (fx? v)
  (bveq (bvand v fx-mask) fx-tag))

(define-symbolic null-tag native_t)
(define-symbolic null-shift native_t)
(define-symbolic null-mask native_t)

(define (null? v)
  (bveq (bvand v null-mask) null-tag))

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
  (xor (ptr? v)
       (char? v)
       (fx? v)
       (bool? v)
       (null? v)))

;; Encoding procedures
(define (fx-encode i)
  (bvor fx-tag (bvshl i fx-shift)))

(define (char-encode c)
  (bvor char-tag (bvshl c char-shift)))

(define (null-encode n)
  (bvor null-tag (bvshl n null-shift)))

(define (bool-encode b)
  (bvor bool-tag (bvshl b bool-shift)))

(assert (or (bool? (bool-encode (bv 1 bw))) (bool? (bool-encode (bv 0 bw))))) 
         
(synthesize #:forall (list value null-value1 null-value2 fx-value)
            #:guarantee (begin
                          (assert (bveq (null-encode null-value1) (null-encode null-value2)))))