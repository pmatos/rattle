 ;
 ; Copyright 2020 Paulo Matos
 ;
 ; Licensed under the Apache License, Version 2.0 (the "License");
 ; you may not use this file except in compliance with the License.
 ; You may obtain a copy of the License at
 ;
 ;     http://www.apache.org/licenses/LICENSE-2.0
 ;
 ; Unless required by applicable law or agreed to in writing, software
 ; distributed under the License is distributed on an "AS IS" BASIS,
 ; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ; See the License for the specific language governing permissions and
 ; limitations under the License.
 ;

#lang racket/base
;; ---------------------------------------------------------------------------------------------------

(require racket/port
         racket/system
         racket/string)

;; ---------------------------------------------------------------------------------------------------

; Read a file that looks like
; X1 => Y1
; --
; X2 => Y2
; --
; ...
;
; Yn can be the string 'error'
; in which case the test passes if the exit code is non-zero.
; Otherwise the test only passes if the exit code is zero and output matches Yn, when given Xn on the
; command line

(struct result (fail? output)
  #:prefab)

(define (run-test cmd t)
  ;; Given a command line string cmd and an x
  ;; it returns an instance with the output of running 'cmd x'
  (call/cc
   (lambda (return)
     (define out
       (with-output-to-string
         (lambda ()
           (unless (system (format "~a '~a'" cmd (test-input t)))
             (return (result #true ""))))))
     (result #false (string-trim out)))))

(struct test (input err? output)
  #:prefab)

(define (read-test t)
  (define-values (input output)
    (apply values (map string-trim (string-split t "=>"))))
  (if (equal? output "error")
      (test input #true "")
      (test input #false output)))

(define (read-file f)
  (apply string
         (reverse
          (call-with-input-file f
            (lambda (in)
              (let loop ([cs '()])
                (define c (read-char in))
                (if (eof-object? c)
                    cs
                    (loop (cons c cs)))))))))

(define (read-tests f)
  (map read-test (string-split (read-file f) "--")))

(define (run-tests cmd tests)
  ;; receives a list of test instances and executes them with cmd
  (define passes
    (for/fold ([pass 0])
              ([t (in-list tests)])
      (printf "Test: ~a ~a => " cmd (test-input t))
      (define r (run-test cmd t))
      (if (result-fail? r)
          (printf "error ")
          (printf "~a " (result-output r)))

      (define pass?
        (or (and (test-err? t) (result-fail? r))
            (and (not (test-err? t)) (equal? (result-output r) (test-output t)))))

      (if pass?
          (printf "PASS~n")
          (printf "FAIL (expected ~a)~n"
                  (if (test-err? t)
                      "error"
                      (test-output t))))

      (+ pass (if pass? 1 0))))
  (printf "DONE ~a/~a~n" passes (length tests))
  (if (= passes (length tests))
      (exit 0)
      (exit 1)))


(module+ main

  (require racket/cmdline)

  (define driver-command-line (make-parameter #f))

  (define testfile
    (command-line
     #:program "test"
     #:once-each
     [("-c") cmd "Command line to run tests with"
      (driver-command-line cmd)]
     #:args (f)
     (unless (file-exists? f)
       (error "file `~a' does not exist" f))
     f))

  (unless (driver-command-line)
    (error "you need to pass -c <command line for driver>"))

  (run-tests (driver-command-line) (read-tests testfile)))
