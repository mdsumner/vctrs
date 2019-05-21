#include "vctrs.h"
#include "dictionary.h"

// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
int32_t ceil2(int32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
}

// Dictonary object ------------------------------------------------------------

// Caller is responsible for PROTECTing x
void dict_init(dictionary* d, SEXP x, bool hashed) {
  d->x = x;

  // assume worst case, that every value is distinct, aiming for a load factor
  // of at most 77%. We round up to power of 2 to ensure quadratic probing
  // strategy works.
  R_len_t size = ceil2(vec_size(x) / 0.77);
  if (size < 16) {
    size = 16;
  }
  // Rprintf("size: %i\n", size);

  d->key = (R_len_t*) R_alloc(size, sizeof(R_len_t));
  memset(d->key, DICT_EMPTY, size * sizeof(R_len_t));

  if (hashed) {
    R_len_t n = vec_size(x);
    d->hash = (uint32_t*) R_alloc(n, sizeof(uint32_t));
    memset(d->hash, 0, n * sizeof(R_len_t));
    hash_fill(d->hash, n, x);
  } else {
    d->hash = NULL;
  }

  d->size = size;
  d->used = 0;
}

void dict_free(dictionary* d) {
  // no cleanup currently needed
}

uint32_t dict_hash_scalar(dictionary* d, SEXP y, R_len_t i) {
  uint32_t hash;

  if (d->hash) {
    if (d->x != y) {
      Rf_error("Internal error: Can't compare hashed vector "
               "with unhashed vector.");
    }
    hash = d->hash[i];
  } else {
    hash = hash_scalar(y, i);
  }

  // Quadratic probing: will try every slot if d->size is power of 2
  // http://research.cs.vt.edu/AVresearch/hashing/quadratic.php
  for (uint32_t k = 0; k < d->size; ++k) {
    uint32_t probe = (hash + k * (k + 1) / 2) % d->size;
    // Rprintf("Probe: %i\n", probe);

    // If we circled back to start, dictionary is full
    if (k > 1 && probe == hash) {
      break;
    }

    // Check for unused slot
    R_len_t idx = d->key[probe];
    if (idx == DICT_EMPTY) {
      return probe;
    }

    // Check for same value as there might be a collision. If there is
    // a collision, next iteration will find another spot using
    // quadratic probing.
    if (equal_scalar(d->x, idx, y, i, true)) {
      return probe;
    }
  }

  Rf_errorcall(R_NilValue, "Internal error: Dictionary is full!");
}

void dict_put(dictionary* d, uint32_t hash, R_len_t i) {
  d->key[hash] = i;
  d->used++;
}

// R interface -----------------------------------------------------------------
// TODO: rename to match R function names
// TODO: separate out into individual files

SEXP vctrs_unique_loc(SEXP x) {
  dictionary d;
  dict_init(&d, x, true);

  growable g;
  growable_init(&g, INTSXP, 256);

  R_len_t n = vec_size(x);
  for (int i = 0; i < n; ++i) {
    uint32_t hash = dict_hash_scalar(&d, x, i);

    if (d.key[hash] == DICT_EMPTY) {
      dict_put(&d, hash, i);
      growable_push_int(&g, i + 1);
    }
  }

  SEXP out = growable_values(&g);
  dict_free(&d);
  growable_free(&g);
  return out;
}

SEXP vctrs_duplicated_any(SEXP x) {
  dictionary d;
  dict_init(&d, x, false);

  bool out = false;
  R_len_t n = vec_size(x);

  for (int i = 0; i < n; ++i) {
    uint32_t hash = dict_hash_scalar(&d, x, i);

    if (d.key[hash] == DICT_EMPTY) {
      dict_put(&d, hash, i);
    } else {
      out = true;
      break;
    }
  }

  dict_free(&d);
  return Rf_ScalarLogical(out);
}

SEXP vctrs_n_distinct(SEXP x) {
  dictionary d;
  dict_init(&d, x, true);

  R_len_t n = vec_size(x);
  for (int i = 0; i < n; ++i) {
    uint32_t hash = dict_hash_scalar(&d, x, i);

    if (d.key[hash] == DICT_EMPTY)
      dict_put(&d, hash, i);
  }

  dict_free(&d);
  return Rf_ScalarInteger(d.used);
}

SEXP vctrs_id(SEXP x) {
  dictionary d;
  dict_init(&d, x, true);

  R_len_t n = vec_size(x);
  SEXP out = PROTECT(Rf_allocVector(INTSXP, n));
  int* p_out = INTEGER(out);

  for (int i = 0; i < n; ++i) {
    uint32_t hash = dict_hash_scalar(&d, x, i);

    if (d.key[hash] == DICT_EMPTY) {
      dict_put(&d, hash, i);
    }
    p_out[i] = d.key[hash] + 1;
  }

  UNPROTECT(1);
  dict_free(&d);
  return out;
}

SEXP vctrs_match(SEXP needles, SEXP haystack) {
  dictionary d;
  dict_init(&d, haystack, false);

  // Load dictionary with haystack
  R_len_t n_haystack = vec_size(haystack);
  for (int i = 0; i < n_haystack; ++i) {
    uint32_t hash = dict_hash_scalar(&d, haystack, i);

    if (d.key[hash] == DICT_EMPTY) {
      dict_put(&d, hash, i);
    }
  }

  // Locate needles
  R_len_t n_needle = vec_size(needles);
  SEXP out = PROTECT(Rf_allocVector(INTSXP, n_needle));
  int* p_out = INTEGER(out);

  for (int i = 0; i < n_needle; ++i) {
    uint32_t hash = dict_hash_scalar(&d, needles, i);
    if (d.key[hash] == DICT_EMPTY) {
      p_out[i] = NA_INTEGER;
    } else {
      p_out[i] = d.key[hash] + 1;
    }
  }
  UNPROTECT(1);
  dict_free(&d);
  return out;
}


SEXP vctrs_in(SEXP needles, SEXP haystack) {
  dictionary d;
  dict_init(&d, haystack, false);

  // Load dictionary with haystack
  R_len_t n_haystack = vec_size(haystack);
  for (int i = 0; i < n_haystack; ++i) {
    uint32_t hash = dict_hash_scalar(&d, haystack, i);

    if (d.key[hash] == DICT_EMPTY) {
      dict_put(&d, hash, i);
    }
  }

  // Locate needles
  R_len_t n_needle = vec_size(needles);
  SEXP out = PROTECT(Rf_allocVector(LGLSXP, n_needle));
  int* p_out = LOGICAL(out);

  for (int i = 0; i < n_needle; ++i) {
    uint32_t hash = dict_hash_scalar(&d, needles, i);
    p_out[i] = (d.key[hash] != DICT_EMPTY);
  }
  UNPROTECT(1);
  dict_free(&d);
  return out;
}

SEXP vctrs_count(SEXP x) {
  dictionary d;
  dict_init(&d, x, true);

  SEXP val = PROTECT(Rf_allocVector(INTSXP, d.size));
  int* p_val = INTEGER(val);

  R_len_t n = vec_size(x);
  for (int i = 0; i < n; ++i) {
    int32_t hash = dict_hash_scalar(&d, x, i);

    if (d.key[hash] == DICT_EMPTY) {
      dict_put(&d, hash, i);
      p_val[hash] = 0;
    }
    p_val[hash]++;
  }

  // Create output
  SEXP out_key = PROTECT(Rf_allocVector(INTSXP, d.used));
  SEXP out_val = PROTECT(Rf_allocVector(INTSXP, d.used));
  int* p_out_key = INTEGER(out_key);
  int* p_out_val = INTEGER(out_val);

  int i = 0;
  for (int hash = 0; hash < d.size; ++hash) {
    if (d.key[hash] == DICT_EMPTY)
      continue;

    p_out_key[i] = d.key[hash] + 1;
    p_out_val[i] = p_val[hash];
    i++;
  }

  SEXP out = PROTECT(Rf_allocVector(VECSXP, 2));
  SET_VECTOR_ELT(out, 0, out_key);
  SET_VECTOR_ELT(out, 1, out_val);
  SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(names, 0, Rf_mkChar("key"));
  SET_STRING_ELT(names, 1, Rf_mkChar("val"));
  Rf_setAttrib(out, R_NamesSymbol, names);

  UNPROTECT(5);
  dict_free(&d);
  return out;
}

SEXP vctrs_duplicated(SEXP x) {
  dictionary d;
  dict_init(&d, x, true);

  SEXP val = PROTECT(Rf_allocVector(INTSXP, d.size));
  int* p_val = INTEGER(val);

  R_len_t n = vec_size(x);
  for (int i = 0; i < n; ++i) {
    int32_t hash = dict_hash_scalar(&d, x, i);

    if (d.key[hash] == DICT_EMPTY) {
      dict_put(&d, hash, i);
      p_val[hash] = 0;
    }
    p_val[hash]++;
  }

  // Create output
  SEXP out = PROTECT(Rf_allocVector(LGLSXP, n));
  int* p_out = LOGICAL(out);

  for (int i = 0; i < n; ++i) {
    int32_t hash = dict_hash_scalar(&d, x, i);
    p_out[i] = p_val[hash] != 1;
  }

  UNPROTECT(2);
  dict_free(&d);
  return out;
}

SEXP vctrs_duplicate_split(SEXP x) {
  dictionary d;
  dict_init(&d, x, true);

  // Tracks the order in which keys are seen
  SEXP tracker = PROTECT(Rf_allocVector(INTSXP, d.size));
  int* p_tracker = INTEGER(tracker);

  // Collects the counts of each key
  SEXP count = PROTECT(Rf_allocVector(INTSXP, d.size));
  int* p_count = INTEGER(count);

  R_len_t n = vec_size(x);

  // Tells us which element of the index list x[i] goes in
  SEXP out_pos = PROTECT(Rf_allocVector(INTSXP, n));
  int* p_out_pos = INTEGER(out_pos);

  // Fill dictionary, out_pos, and count
  for (int i = 0; i < n; ++i) {
    uint32_t hash = dict_hash_scalar(&d, x, i);

    if (d.key[hash] == DICT_EMPTY) {
      p_tracker[hash] = d.used;
      dict_put(&d, hash, i);
      p_count[hash] = 0;
    }

    p_out_pos[i] = p_tracker[hash];
    p_count[hash]++;
  }

  SEXP out_key = PROTECT(Rf_allocVector(INTSXP, d.used));
  int* p_out_key = INTEGER(out_key);

  SEXP out_idx = PROTECT(Rf_allocVector(VECSXP, d.used));

  SEXP counters = PROTECT(Rf_allocVector(INTSXP, d.used));
  int* p_counters = INTEGER(counters);
  memset(p_counters, 0, d.used * sizeof(int));

  // Set up empty index container
  for (int hash = 0; hash < d.size; ++hash) {
    if (d.key[hash] == DICT_EMPTY) {
      continue;
    }

    SET_VECTOR_ELT(out_idx, p_tracker[hash], Rf_allocVector(INTSXP, p_count[hash]));
  }

  // Fill index container and key locations
  for (int i = 0; i < n; ++i) {
    int j = p_out_pos[i];
    int hash = p_counters[j];

    if (hash == 0) {
      p_out_key[j] = i + 1;
    }

    INTEGER(VECTOR_ELT(out_idx, j))[hash] = i + 1;
    p_counters[j] = hash + 1;
  }

  // Construct output
  SEXP out = PROTECT(Rf_allocVector(VECSXP, 2));
  SET_VECTOR_ELT(out, 0, out_key);
  SET_VECTOR_ELT(out, 1, out_idx);
  SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(names, 0, Rf_mkChar("key"));
  SET_STRING_ELT(names, 1, Rf_mkChar("idx"));
  Rf_setAttrib(out, R_NamesSymbol, names);

  UNPROTECT(8);
  dict_free(&d);
  return out;
}
