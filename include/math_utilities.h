#ifndef MATH_UTILITIES_H
#define MATH_UTILITIES_H

/** @defgroup math_utils Utility macros */
/** @{ */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(a, min, max) ((a) < (min) ? (min) : ((a) > (max) ? (max) : (a)))
#define LERP(a, b, t) (((b) - (a)) * (t) + (a))
/** @} */

#endif // MATH_UTILITIES_H