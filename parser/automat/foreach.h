/* title: Foreach-Iterator
   Allows to iterate over content of container
   with simple <foreach> macro.
   Using the iterator interface directly is also possible

   > int  initfirst_containeriterator (container_iterator_t * iter, container_t * container) ;
   > bool next_containeriterator (container_iterator_t * iter, container_node_t ** node) ;
   >
   > int  initlast_containeriterator (container_iterator_t * iter, container_t * container) ;
   > bool prev_containeriterator (container_iterator_t * iter, container_node_t ** node) ;
   >
   > int  free_containeriterator (container_iterator_t * iter) ;

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/foreach.h
    Header file <Foreach-Iterator>.
*/
#ifndef CKERN_DS_FOREACH_HEADER
#define CKERN_DS_FOREACH_HEADER


// section: Functions

/* define: foreach
 * Iterates over all elements from first to last stored in a container.
 * In a sorted container the first element equals the smallest element
 * and therefore the iteration is done in ascending order.
 *
 * Mnemonic:
 * > foreach variable in container {         // Intention
 * > foreach(_type, variable, &container) {  // Macro syntax
 *
 * Changing Container:
 * During iteration do not change the content of the container
 * except if the documentation of the iterator implementation allows it.
 *
 * Exiting loop:
 * It is possible to exit <foreach> loop by using *break*.
 * The initialized iterator is also freed in this case.
 * If you exit the loop by use of goto or any other exception handling
 * mechanism the iterator is *not* freed which may result in memory leaks.
 *
 * Parameter:
 * _fctsuffix - The suffix of the container interface functions.
 *              This name is used to access the types and iterator functions.
 * varname    - The name of the variable which iterates over all contained elements.
 *              The used iterator has the generated name "iter_varname" with varname
 *              replaced with the given argument value. You can access it to modify it.
 * container  - Pointer to container which contains all elements.
 * ...        - Any additional parameter function initfirst##_fctsuffix##iterator takes
 *
 * Explanation:
 * A container type which wants to offer <foreach> functionality must implement the
 * following iterator interface:
 *
 * > typedef iterator_t iteratortype##_fctsuffix ;
 * > typedef node_t   * iteratedtype##_fctsuffix ;
 * > int  initfirst##_fctsuffix##iterator(iterator_t * iter, container_t * container) ;
 * > int  free##_fctsuffix##iterator(iterator_t * iter) ;
 * > bool next##_fctsuffix##iterator(iterator_t * iter, node_t ** node) ;
 *
 * The function like typedefs iteratortype##_fctsuffix and iteratedtype##_fctsuffix are used to
 * get the type of the returned node from next##_fctsuffix##iterator and the type of
 * the iterator.
 *
 * See <slist_t> for an example.
 */
#define foreach(_fsuffix, varname, ...)                                       \
   for (iteratortype##_fsuffix iter_##varname;                                \
        0 == initfirst##_fsuffix##iterator(& iter_##varname, __VA_ARGS__);)   \
   for (iteratedtype##_fsuffix varname;;                                      \
        (__extension__({ (void)free##_fsuffix##iterator(&iter_##varname);     \
                           break/*breaks outermost loop*/; })))               \
   while (next##_fsuffix##iterator(&iter_##varname, &varname))


/* define: foreachReverse
 * Iterates over all elements from last to first stored in a container.
 * In a sorted container the last element equals the biggest element
 * and therefore the iteration is done in descending order.
 *
 * Mnemonic:
 * > foreachReverse variable in container {         // Intention
 * > foreachReverse(_type, variable, &container) {  // Macro syntax
 *
 * Changing Container:
 * During iteration do not change the content of the container
 * except if the documentation of the iterator implementation allows it.
 *
 * Exiting loop:
 * It is possible to exit <foreachReverse> loop by using *break*.
 * The initialized iterator is also freed in this case.
 * If you exit the loop by use of goto or any other exception handling
 * mechanism the iterator is *not* freed which may result in memory leaks.
 *
 * Parameter:
 * _fctsuffix - The suffix of the container interface functions.
 *              This name is used to access the types and iterator functions.
 * varname    - The name of the variable which iterates over all contained elements.
 *              The used iterator has the generated name "iter_varname" with varname
 *              replaced with the given argument value. You can access it to modify it.
 * container  - Pointer to container which contains all elements.
 * ...        - Any additional parameter function initlast##_fctsuffix##iterator takes
 *
 * Explanation:
 * A container type which wants to offer <foreachReverse> functionality must implement the
 * following iterator interface:
 *
 * > typedef iterator_t iteratortype##_fctsuffix ;
 * > typedef node_t  *  iteratedtype##_fctsuffix ;
 * > int  initlast##_fctsuffix##iterator(iterator_t * iter, container_t * container) ;
 * > int  free##_fctsuffix##iterator(iterator_t * iter) ;
 * > bool prev##_fctsuffix##iterator(iterator_t * iter, node_t ** node) ;
 *
 * The function like typedefs iteratortype##_fctsuffix and iteratedtype##_fctsuffix are used to
 * get the type of the returned node from prev##_fctsuffix##iterator and the type of
 * the iterator.
 *
 * See <dlist_t> for an example.
 */
#define foreachReverse(_fsuffix, varname, ...)                                \
   for (iteratortype##_fsuffix iter_##varname;                                \
        0 == initlast##_fsuffix##iterator(& iter_##varname, __VA_ARGS__);)    \
   for (iteratedtype##_fsuffix varname;;                                      \
        (__extension__({ (void)free##_fsuffix##iterator(&iter_##varname);     \
                         break/*breaks outermost loop*/; })))                 \
   while (prev##_fsuffix##iterator(&iter_##varname, &varname))

#endif
