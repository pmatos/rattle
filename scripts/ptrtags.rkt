#lang rosette/safe

(current-bitwidth #f)
(define bw 32) (define ptrmask #x7)
;(define bw 64) (define ptrmask #xf)

(define native_t (bitvector bw))

(define-symbolic ptr native_t)

(define-symbolic null-value1 native_t)
(define-symbolic null-value2 native_t)

(define-symbolic bool-value boolean?)

(define-symbolic fx-value integer?)

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

(define (null-encode n)
  (bvor null-tag (bvshl n null-shift)))

(define (bool-encode b)
  (if b
      (bvor bool-tag (bvshl (bv 1 bw) bool-shift))
      (bvor bool-tag (bvshl (bv 0 bw) bool-shift))))

(synthesize
 #:forall (list bool-value fx-value char-value ptr)
 #:guarantee
 (begin
   (assert (and (valid? (bool-encode bool-value)) (bool? (bool-encode bool-value))))
   (assert (=> (>= (- (expt 2 (- bw 1)))
                   fx-value
                   (- (expt 2 (- bw 1)) 1))
               (and (valid? (fx-encode fx-value)) (fx? (fx-encode fx-value)))))
   (assert (=> (ptr? ptr) (valid? ptr)))
   (assert (=> (>= 0 char-value 255)
               (and (valid? (char-encode char-value)) (char? (char-encode char-value)))))))