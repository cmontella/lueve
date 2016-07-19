struct bag {
    table listeners;
    table delta_listeners;
    table eav;
    table ave;
    uuid u;
    int count;
    table implications;
    heap h;
};

#define s_eav 0x0
#define s_eAv 0x2
#define s_eAV 0x3
#define s_Eav 0x4
#define s_EAv 0x6
#define s_EAV 0x7

value lookupv(bag b, uuid e, estring a);
void edb_scan(bag b, int sig, listener f, value e, value a, value v);

table edb_implications();
void edb_register_implication(bag b, node n);
void edb_remove_implication(bag b, node n);
void edb_clear_implications(bag b);
uuid edb_uuid(bag b);
int edb_size(bag b);
void destroy_bag(bag b);

typedef closure(bag_handler, bag);

void register_listener(bag e, thunk t);
void deregister_listener(bag e, thunk t);
void register_delta_listener(bag e, thunk t);
void deregister_delta_listener(bag e, thunk t);

#define bag_foreach(__b, __e, __a, __v, __c)\
    table_foreach((__b)->eav, __e, __avl) \
    table_foreach((table)__avl, __a, __vl)\
    table_foreach((table)__vl, __v, __cv)\
    for(long __c = (long)__cv , __z = 0; __z == 0; __z++)

long count_of(bag b, value e, value a, value v);
void edb_insert(bag b, value e, value a, value v, multiplicity m);
bag create_bag(heap, uuid);
void edb_remove(bag b, value e, value a, value v);
void edb_set(bag b, value e, value a, value v);


#define bag_foreach_av(__b, __e, __a, __v, __c)\
    for(table __av = (table)table_find((__b)->eav, __e); __av; __av = 0)  \
    table_foreach((table)__av, __a, __vl)\
    table_foreach((table)__vl, __v, __cv)\
    for(long __c = (long)__cv , __z = 0; __z == 0; __z++)

#define bag_foreach_e(__b, __e, __a, __v, __c)\
    for(table __avt = (table)table_find((__b)->ave, __a),\
               __et = __avt?(table)table_find(__avt, __a):0; __et; __et = 0)   \
    table_foreach((table)__et, __e, __cv)\
    for(long __c = (long)__cv , __z = 0; __z == 0; __z++)




