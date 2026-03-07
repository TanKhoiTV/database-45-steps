// include/table/row.h
#pragma once

/**
 * @file row.h
 * @brief Type alias for an ordered sequence of @ref Cell values.
 */

#include "table/cell.h"  // Cell
#include <vector>        // std::vector

/**
 * @brief An ordered, randomly-accessible sequence of @ref Cell values.
 *
 * Column positions in a `Row` correspond 1-to-1 with the columns defined
 * in the associated @ref Schema.  A `Row` carries no schema reference of
 * its own; callers are responsible for keeping the two in sync.
 */
using Row = std::vector<Cell>;
