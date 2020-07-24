#include "order-truelength.h"
#include "utils.h"

/*
 * See the notes in the character ordering section at the top of `order.c`
 * for more details on how TRUELENGTH is used to detect unique strings.
 *
 * The helpers here are somewhat equivalent to the following from R's `order()`
 * https://github.com/wch/r-source/blob/91b4507bf6040c0167fc5b6037c202c8cbd98afd/src/main/radixsort.c#L66-L123
 */

// -----------------------------------------------------------------------------

/*
 * Construct a new `truelength_info`
 *
 * Pair with `PROTECT_TRUELENGTH_INFO()` in the caller
 */
struct truelength_info new_truelength_info(R_xlen_t max_size_alloc) {
  return (struct truelength_info) {
    .strings = R_NilValue,
    .lengths = R_NilValue,
    .uniques = R_NilValue,
    .sizes = R_NilValue,
    .sizes_aux = R_NilValue,

    .size_alloc = 0,
    .max_size_alloc = max_size_alloc,
    .size_used = 0,

    .max_string_size = 0,

    .reencode = false
  };
}

// -----------------------------------------------------------------------------

/*
 * Reset the truelengths of all unique strings captured in `strings` using
 * the original truelengths in `lengths`.
 *
 * Will be called after each character data frame column is processed, and
 * at the end of `chr_order()` for a single character vector.
 */
void truelength_reset(struct truelength_info* p_truelength_info) {
  R_xlen_t size = p_truelength_info->size_used;

  for (R_xlen_t i = 0; i < size; ++i) {
    SEXP string = p_truelength_info->p_strings[i];
    R_xlen_t length = p_truelength_info->p_lengths[i];

    SET_TRUELENGTH(string, length);
  }

  // Also reset vector specific details
  p_truelength_info->size_used = 0;
  p_truelength_info->max_string_size = 0;
  p_truelength_info->reencode = false;
}

// -----------------------------------------------------------------------------

static void truelength_realloc(struct truelength_info* p_truelength_info);

/*
 * Saves a unique CHARSXP `x` along with its original truelength and
 * its "size" (i.e the number of characters). Will be reset later with
 * `truelength_reset()`.
 */
void truelength_save(struct truelength_info* p_truelength_info,
                     SEXP x,
                     R_xlen_t truelength,
                     R_xlen_t size) {
  // Reallocate as needed
  if (p_truelength_info->size_used == p_truelength_info->size_alloc) {
    truelength_realloc(p_truelength_info);
  }

  // Push `x` and `length`
  p_truelength_info->p_strings[p_truelength_info->size_used] = x;
  p_truelength_info->p_lengths[p_truelength_info->size_used] = truelength;
  p_truelength_info->p_uniques[p_truelength_info->size_used] = x;
  p_truelength_info->p_sizes[p_truelength_info->size_used] = size;

  // Bump number of used slots
  ++p_truelength_info->size_used;
}

// -----------------------------------------------------------------------------

static R_xlen_t truelength_realloc_size(struct truelength_info* p_truelength_info);

static SEXP truelength_lengths_extend(const R_xlen_t* p_lengths, R_xlen_t size_old, R_xlen_t size_new);

/*
 * Extend the vectors in `truelength_info`.
 * Reprotects itself.
 */
static
void truelength_realloc(struct truelength_info* p_truelength_info) {
  R_xlen_t size = truelength_realloc_size(p_truelength_info);

  // Reallocate
  p_truelength_info->strings = p_chr_resize(
    p_truelength_info->p_strings,
    p_truelength_info->size_used,
    size
  );
  // Reprotect
  REPROTECT(p_truelength_info->strings, p_truelength_info->strings_pi);
  // Update pointer
  p_truelength_info->p_strings = STRING_PTR(p_truelength_info->strings);

  // Reallocate
  p_truelength_info->lengths = truelength_lengths_extend(
    p_truelength_info->p_lengths,
    p_truelength_info->size_used,
    size
  );
  // Reprotect
  REPROTECT(p_truelength_info->lengths, p_truelength_info->lengths_pi);
  // Update pointer
  p_truelength_info->p_lengths = (R_xlen_t*) RAW(p_truelength_info->lengths);

  // Reallocate
  p_truelength_info->uniques = p_chr_resize(
    p_truelength_info->p_uniques,
    p_truelength_info->size_used,
    size
  );
  // Reprotect
  REPROTECT(p_truelength_info->uniques, p_truelength_info->uniques_pi);
  // Update pointer
  p_truelength_info->p_uniques = STRING_PTR(p_truelength_info->uniques);

  // Reallocate
  p_truelength_info->sizes = p_int_resize(
    p_truelength_info->p_sizes,
    p_truelength_info->size_used,
    size
  );
  // Reprotect
  REPROTECT(p_truelength_info->sizes, p_truelength_info->sizes_pi);
  // Update pointer
  p_truelength_info->p_sizes = INTEGER(p_truelength_info->sizes);

  // Reallocate
  p_truelength_info->sizes_aux = p_int_resize(
    p_truelength_info->p_sizes_aux,
    p_truelength_info->size_used,
    size
  );
  // Reprotect
  REPROTECT(p_truelength_info->sizes_aux, p_truelength_info->sizes_aux_pi);
  // Update pointer
  p_truelength_info->p_sizes_aux = INTEGER(p_truelength_info->sizes_aux);

  // Update size
  p_truelength_info->size_alloc = size;
}

static
SEXP truelength_lengths_extend(const R_xlen_t* p_lengths, R_xlen_t size_old, R_xlen_t size_new) {
  SEXP out = PROTECT(Rf_allocVector(RAWSXP, size_new * sizeof(R_xlen_t)));
  R_xlen_t* p_out = (R_xlen_t*) RAW(out);

  memcpy(p_out, p_lengths, size_old * sizeof(R_xlen_t));

  UNPROTECT(1);
  return out;
}

// -----------------------------------------------------------------------------

static
R_xlen_t truelength_realloc_size(struct truelength_info* p_truelength_info) {
  R_xlen_t size_alloc = p_truelength_info->size_alloc;
  R_xlen_t max_size_alloc = p_truelength_info->max_size_alloc;

  // First allocation
  if (size_alloc == 0) {
    if (TRUELENGTH_SIZE_ALLOC_DEFAULT < max_size_alloc) {
      return TRUELENGTH_SIZE_ALLOC_DEFAULT;
    } else {
      return max_size_alloc;
    }
  }

  // Avoid potential overflow when doubling size
  uint64_t new_size_alloc = ((uint64_t) size_alloc) * 2;

  // Clamp maximum allocation size to the size of the input
  if (new_size_alloc > max_size_alloc) {
    return max_size_alloc;
  }

  // Can now safely cast back to `R_xlen_t`
  return (R_xlen_t) new_size_alloc;
}
