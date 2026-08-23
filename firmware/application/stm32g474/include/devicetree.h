#ifndef CHASSIS_DEVICETREE_H
#define CHASSIS_DEVICETREE_H

#include "devicetree_generated.h"

#define DT_CAT_INNER(a, b) a##b
#define DT_CAT(a, b) DT_CAT_INNER(a, b)
#define DT_CAT4_INNER(a, b, c, d) a##b##c##d
#define DT_CAT4(a, b, c, d) DT_CAT4_INNER(a, b, c, d)
#define DT_ARG_PLACEHOLDER_1 0,
#define DT_TAKE_SECOND_ARG(_ignored, value, ...) value
#define DT_IS_ENABLED_RESOLVE(value) \
  DT_TAKE_SECOND_ARG(DT_CAT(DT_ARG_PLACEHOLDER_, value) 1, 0)
#define DT_IS_ENABLED(value) DT_IS_ENABLED_RESOLVE(value)

#define DT_NODELABEL_RESOLVE(label) DT_CAT(DT_NODELABEL_, label)
#define DT_NODELABEL(label) DT_NODELABEL_RESOLVE(label)
#define DT_ALIAS_RESOLVE(alias) DT_CAT(DT_ALIAS_, alias)
#define DT_ALIAS(alias) DT_ALIAS_RESOLVE(alias)
#define DT_CHOSEN_RESOLVE(chosen) DT_CAT(DT_CHOSEN_, chosen)
#define DT_CHOSEN(chosen) DT_CHOSEN_RESOLVE(chosen)
#define DT_PROP_RESOLVE(node, prop) DT_CAT4(DT_PROP_, node, _, prop)
#define DT_PROP(node, prop) DT_PROP_RESOLVE(node, prop)
#define DT_PROP_EXISTS_RESOLVE(node, prop) \
  DT_CAT4(DT_PROP_, node, _, DT_CAT(prop, _EXISTS))
#define DT_PROP_EXISTS(node, prop) DT_PROP_EXISTS_RESOLVE(node, prop)
#define DT_NODE_HAS_STATUS_RESOLVE(node, status) \
  DT_CAT4(DT_NODE_, node, _STATUS_, status)
#define DT_NODE_HAS_STATUS(node, status) \
  DT_NODE_HAS_STATUS_RESOLVE(node, status)
#define DT_PROP_LEN_RESOLVE(node, prop) \
  DT_CAT4(DT_PROP_, node, _, DT_CAT(prop, _LEN))
#define DT_PROP_LEN(node, prop) DT_PROP_LEN_RESOLVE(node, prop)
#define DT_PROP_BY_IDX_NAME(node, prop, index) \
  DT_CAT4(DT_PROP_, node, _, DT_CAT4(prop, _IDX_, index, ))
#define DT_PROP_BY_IDX_RESOLVE(node, prop, index) \
  DT_PROP_BY_IDX_NAME(node, prop, index)
#define DT_PROP_BY_IDX(node, prop, index) \
  DT_PROP_BY_IDX_RESOLVE(node, prop, index)

#define DT_PHA_LEN_NAME(node, prop) \
  DT_CAT4(DT_PHA_, node, _, DT_CAT(prop, _LEN))
#define DT_PHA_LEN(node, prop) DT_PHA_LEN_NAME(node, prop)
#define DT_PHA_CONTROLLER_NAME(node, prop, index) \
  DT_CAT4(DT_PHA_, node, _, DT_CAT4(prop, _CONTROLLER_, index, ))
#define DT_PHA_CONTROLLER_BY_IDX(node, prop, index) \
  DT_PHA_CONTROLLER_NAME(node, prop, index)
#define DT_PHA_PIN_NAME(node, prop, index) \
  DT_CAT4(DT_PHA_, node, _, DT_CAT4(prop, _PIN_, index, ))
#define DT_PHA_PIN_BY_IDX(node, prop, index) DT_PHA_PIN_NAME(node, prop, index)
#define DT_PHA_FLAGS_NAME(node, prop, index) \
  DT_CAT4(DT_PHA_, node, _, DT_CAT4(prop, _FLAGS_, index, ))
#define DT_PHA_FLAGS_BY_IDX(node, prop, index) \
  DT_PHA_FLAGS_NAME(node, prop, index)

#define DT_PROP_OR_SELECT_1(node, prop, default_value) DT_PROP(node, prop)
#define DT_PROP_OR_SELECT_0(node, prop, default_value) default_value
#define DT_PROP_OR_SELECT(exists, node, prop, default_value) \
  DT_CAT(DT_PROP_OR_SELECT_, exists)(node, prop, default_value)
#define DT_PROP_OR(node, prop, default_value) \
  DT_PROP_OR_SELECT(DT_IS_ENABLED(DT_PROP_EXISTS(node, prop)), node, prop, \
                    default_value)

#endif
