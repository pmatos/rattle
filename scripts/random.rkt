#lang racket/base
;; ---------------------------------------------------------------------------------------------------

(require binaryio)

;; ---------------------------------------------------------------------------------------------------

(define fx-max 4611686018427387903)
(define fx-min -4611686018427387904)

(define (rnd-fixnum) ; -> fixnum?
  (define bs (crypto-random-bytes 8))
  (+ (mod (bytes->integer bs) (sub1 (expt 2 7)))
     fx-min))

(define (rnd-char) ; -> char?
  (integer->char (random 256)))

(define (rnd-bool) ; -> boolean?
  (if (zero? (random 2))
      #f
      #t))


(define (rnd-prim)
  (define prims
    '(fxadd1
      fxsub1
      fxzero?
      char->fixnum
      fixnum->char))
