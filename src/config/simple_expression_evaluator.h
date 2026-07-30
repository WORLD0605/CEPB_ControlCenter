#ifndef CONFIG_SIMPLE_EXPRESSION_EVALUATOR_H
#define CONFIG_SIMPLE_EXPRESSION_EVALUATOR_H

#include <QString>

namespace configtool {

// Mirrors LogicCenter's lightweight expression syntax:
// floating-point constants, + - * /, parentheses, unary +/-,
// sqrt(x), sqr(x), square(x), and pow(x, y).
bool evaluateSimpleExpression(const QString &expression, double *result);

} // namespace configtool

#endif
