# assertion failures are explained

    Code
      vec_assert(lgl(), chr())
    Condition
      Error:
      ! `lgl()` must be a vector with type <character>.
      Instead, it has type <logical>.

---

    Code
      vec_assert(lgl(), factor())
    Condition
      Error:
      ! `lgl()` must be a vector with type <factor<>>.
      Instead, it has type <logical>.

---

    Code
      vec_assert(lgl(), factor(levels = "foo"))
    Condition
      Error:
      ! `lgl()` must be a vector with type <factor<c1562>>.
      Instead, it has type <logical>.

---

    Code
      vec_assert(factor(levels = "bar"), factor(levels = "foo"))
    Condition
      Error:
      ! `factor(levels = "bar")` must be a vector with type <factor<c1562>>.
      Instead, it has type <factor<9f154>>.

---

    Code
      vec_assert(factor(), chr())
    Condition
      Error:
      ! `factor()` must be a vector with type <character>.
      Instead, it has type <factor<>>.

---

    Code
      vec_assert(lgl(), data.frame())
    Condition
      Error:
      ! `lgl()` must be a vector with type <data.frame<>>.
      Instead, it has type <logical>.

---

    Code
      vec_assert(lgl(), data.frame(x = 1))
    Condition
      Error:
      ! `lgl()` must be a vector with type <data.frame<x:double>>.
      Instead, it has type <logical>.

---

    Code
      vec_assert(lgl(), data.frame(x = 1, y = 2))
    Condition
      Error:
      ! `lgl()` must be a vector with type:
      
        <data.frame<
          x: double
          y: double
        >>
      
      Instead, it has type <logical>.

---

    Code
      vec_assert(data.frame(), chr())
    Condition
      Error:
      ! `data.frame()` must be a vector with type <character>.
      Instead, it has type <data.frame<>>.

---

    Code
      vec_assert(data.frame(x = 1), chr())
    Condition
      Error:
      ! `data.frame(x = 1)` must be a vector with type <character>.
      Instead, it has type <data.frame<x:double>>.

---

    Code
      vec_assert(data.frame(x = 1), data.frame(x = "foo"))
    Condition
      Error:
      ! `data.frame(x = 1)` must be a vector with type <data.frame<x:character>>.
      Instead, it has type <data.frame<x:double>>.

---

    Code
      vec_assert(data.frame(x = 1), data.frame(x = "foo", y = 2))
    Condition
      Error:
      ! `data.frame(x = 1)` must be a vector with type:
      
        <data.frame<
          x: character
          y: double
        >>
      
      Instead, it has type <data.frame<x:double>>.

---

    Code
      vec_assert(data.frame(x = 1, y = 2), chr())
    Condition
      Error:
      ! `data.frame(x = 1, y = 2)` must be a vector with type <character>.
      Instead, it has type:
      
        <data.frame<
          x: double
          y: double
        >>

---

    Code
      vec_assert(data.frame(x = 1, y = 2), data.frame(x = "foo"))
    Condition
      Error:
      ! `data.frame(x = 1, y = 2)` must be a vector with type <data.frame<x:character>>.
      Instead, it has type:
      
        <data.frame<
          x: double
          y: double
        >>

---

    Code
      vec_assert(data.frame(x = 1, y = 2), data.frame(x = "foo", y = 2))
    Condition
      Error:
      ! `data.frame(x = 1, y = 2)` must be a vector with type:
      
        <data.frame<
          x: character
          y: double
        >>
      
      Instead, it has type:
      
        <data.frame<
          x: double
          y: double
        >>

# vec_assert() validates `size` (#1470)

    Code
      (expect_error(vec_assert(1, size = c(2, 3))))
    Output
      <error/rlang_error>
      Error in `vec_assert()`:
      ! `size` must be length 1, not length 2.
    Code
      (expect_error(vec_assert(1, size = 1.5)))
    Output
      <error/vctrs_error_cast_lossy>
      Error in `vec_assert()`:
      ! Can't convert from `size` <double> to <integer> due to loss of precision.
      * Locations: 1
    Code
      (expect_error(vec_assert(1, size = "x")))
    Output
      <error/vctrs_error_incompatible_type>
      Error in `vec_assert()`:
      ! Can't convert `size` <character> to <integer>.

# list_all_vectors() works

    Code
      (expect_error(list_all_vectors(env())))
    Output
      <error/rlang_error>
      Error in `list_all_vectors()`:
      ! `x` must be a list, not an environment.

# vec_check_list() works

    Code
      my_function <- (function(my_arg) vec_check_list(my_arg))
      (expect_error(my_function(env())))
    Output
      <error/rlang_error>
      Error in `my_function()`:
      ! `my_arg` must be a list, not an environment.

# vec_check_list() and list_check_all_vectors() work

    Code
      my_function <- (function(my_arg) list_check_all_vectors(my_arg))
      (expect_error(my_function(env())))
    Output
      <error/rlang_error>
      Error in `list_check_all_vectors()`:
      ! `x` must be a list, not an environment.
    Code
      (expect_error(my_function(list(1, env()))))
    Output
      <error/vctrs_error_scalar_type>
      Error in `my_function()`:
      ! `my_arg[[2]]` must be a vector, not an environment.
    Code
      (expect_error(my_function(list(1, name = env()))))
    Output
      <error/vctrs_error_scalar_type>
      Error in `my_function()`:
      ! `my_arg$name` must be a vector, not an environment.
    Code
      (expect_error(my_function(list(1, foo = env()))))
    Output
      <error/vctrs_error_scalar_type>
      Error in `my_function()`:
      ! `my_arg$foo` must be a vector, not an environment.

# informative messages when 1d array doesn't match vector

    Code
      (expect_error(vec_assert(x, int())))
    Output
      <error/vctrs_error_assert_ptype>
      Error:
      ! `x` must be a vector with type <integer>.
      Instead, it has type <integer[1d]>.
