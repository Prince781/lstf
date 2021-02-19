ptr_list = executable('ptr-list',
  dependencies: [data_structures, util],
  c_args: c_args,
  sources: ['ptr-list.c'],
  install: false,
)

test('ptr-list', ptr_list, suite: 'data-structures')

ptr_hashmap = executable('ptr-hashmap',
  dependencies: [data_structures, util, json],
  c_args: c_args,
  sources: ['ptr-hashmap.c'],
  install: false,
)

test('ptr-hashmap', ptr_hashmap, suite: 'data-structures')

ptr_hashmap_large = executable('ptr-hashmap-large',
  dependencies: [data_structures, util],
  c_args: c_args,
  sources: ['ptr-hashmap-large.c'],
  install: false,
)

test('ptr-hashmap-large', ptr_hashmap_large, suite: 'data-structures')

intset = executable('intset',
  dependencies: [data_structures, util],
  c_args: c_args,
  sources: ['intset.c'],
  install: false,
)

test('intset', intset, suite: 'data-structures')
