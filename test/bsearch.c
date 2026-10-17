#include "../include/stdlib.h"

static int compare_ints(const void *key, const void *element) {
  int wanted = *(const int *)key;
  int found = *(const int *)element;
  return (wanted > found) - (wanted < found);
}

struct record {
  int id;
  const char *name;
};

static int compare_record_ids(const void *key, const void *element) {
  int wanted = *(const int *)key;
  int found = ((const struct record *)element)->id;
  return (wanted > found) - (wanted < found);
}

int main(void) {
  const int sorted[] = {-20, -3, 0, 4, 9, 15, 42};
  int key = -20;
  if (bsearch(&key, sorted, sizeof(sorted) / sizeof(sorted[0]), sizeof(sorted[0]), compare_ints) != &sorted[0])
    return 1;

  key = 42;
  if (bsearch(&key, sorted, sizeof(sorted) / sizeof(sorted[0]), sizeof(sorted[0]), compare_ints) != &sorted[6])
    return 2;

  key = 4;
  if (bsearch(&key, sorted, sizeof(sorted) / sizeof(sorted[0]), sizeof(sorted[0]), compare_ints) != &sorted[3])
    return 3;

  /* Missing keys on both ends and between elements must return null. */
  key = -21;
  if (bsearch(&key, sorted, 7, sizeof(sorted[0]), compare_ints))
    return 4;
  key = 43;
  if (bsearch(&key, sorted, 7, sizeof(sorted[0]), compare_ints))
    return 5;
  key = 5;
  if (bsearch(&key, sorted, 7, sizeof(sorted[0]), compare_ints))
    return 6;

  /* Empty ranges must not dereference their base pointer or invoke compare. */
  key = 1;
  if (bsearch(&key, 0, 0, sizeof(int), compare_ints))
    return 7;

  /* Element size is supplied independently of the key's representation. */
  const struct record records[] = {
      {2, "two"},
      {5, "five"},
      {8, "eight"},
      {13, "thirteen"},
  };
  key = 8;
  const struct record *record =
      (const struct record *)bsearch(&key, records, 4, sizeof(records[0]), compare_record_ids);
  if (record != &records[2] || record->name[0] != 'e')
    return 8;

  /* If keys are duplicated, accept any matching element in the equal run. */
  const int duplicates[] = {1, 2, 2, 2, 3};
  key = 2;
  int *duplicate =
      (int *)bsearch(&key, duplicates, sizeof(duplicates) / sizeof(duplicates[0]), sizeof(duplicates[0]),
                     compare_ints);
  if (!duplicate || *duplicate != key || duplicate < duplicates + 1 || duplicate > duplicates + 3)
    return 9;

  return 0;
}
