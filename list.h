#ifndef _ish_list_h_included
#define _ish_list_h_included

#define ISH_LIST_FOR_EACH(type, list, item) \
  for (type item = list; item; item = item->next)

#define ISH_LIST_PREPEND(type, list, item) \
  do { \
    item->next = list; \
    list = item; \
  } while (0);

#define ISH_LIST_APPEND(type, list, item) \
  do { \
    if (!list) { \
      list = item; \
    } else { \
      type p = list; \
      while (p->next) { \
        p = p->next; \
      } \
      p->next = item; \
    } \
  } while (0);

#endif
