#include "config/simple_expression_evaluator.h"

#include <cctype>
#include <cmath>
#include <limits>
#include <string>

namespace configtool {
namespace {

class SimpleExpressionParser
{
public:
    explicit SimpleExpressionParser(const std::string &expression)
        : m_expression(expression)
    {
    }

    double parse()
    {
        m_position = 0;
        m_error = false;
        const double value = parseExpression();
        skipSpaces();
        if (m_error || m_position != m_expression.size()
            || std::isnan(value) || std::isinf(value)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return value;
    }

private:
    void skipSpaces()
    {
        while (m_position < m_expression.size()
               && std::isspace(static_cast<unsigned char>(m_expression[m_position]))) {
            ++m_position;
        }
    }

    double parseNumber()
    {
        skipSpaces();
        const std::size_t start = m_position;
        bool seenDigit = false;
        while (m_position < m_expression.size()
               && (std::isdigit(static_cast<unsigned char>(m_expression[m_position]))
                   || m_expression[m_position] == '.')) {
            if (std::isdigit(static_cast<unsigned char>(m_expression[m_position]))) {
                seenDigit = true;
            }
            ++m_position;
        }
        if (!seenDigit) {
            m_error = true;
            return 0.0;
        }

        try {
            return std::stod(m_expression.substr(start, m_position - start));
        } catch (...) {
            m_error = true;
            return 0.0;
        }
    }

    double parseExpression()
    {
        double value = parseTerm();
        while (!m_error) {
            skipSpaces();
            if (m_position >= m_expression.size()) {
                break;
            }
            const char operation = m_expression[m_position];
            if (operation != '+' && operation != '-') {
                break;
            }
            ++m_position;
            const double right = parseTerm();
            value = operation == '+' ? value + right : value - right;
        }
        return value;
    }

    double parseTerm()
    {
        double value = parseFactor();
        while (!m_error) {
            skipSpaces();
            if (m_position >= m_expression.size()) {
                break;
            }
            const char operation = m_expression[m_position];
            if (operation != '*' && operation != '/') {
                break;
            }
            ++m_position;
            const double right = parseFactor();
            if (operation == '/' && right == 0.0) {
                m_error = true;
                return std::numeric_limits<double>::quiet_NaN();
            }
            value = operation == '*' ? value * right : value / right;
        }
        return value;
    }

    double parseFactor()
    {
        skipSpaces();
        if (m_position >= m_expression.size()) {
            m_error = true;
            return 0.0;
        }

        const char current = m_expression[m_position];
        if (current == '+' || current == '-') {
            ++m_position;
            const double value = parseFactor();
            return current == '-' ? -value : value;
        }
        if (current == '(') {
            ++m_position;
            const double value = parseExpression();
            skipSpaces();
            if (m_position >= m_expression.size() || m_expression[m_position] != ')') {
                m_error = true;
                return 0.0;
            }
            ++m_position;
            return value;
        }
        if (std::isalpha(static_cast<unsigned char>(current)) || current == '_') {
            return parseFunction();
        }
        return parseNumber();
    }

    double parseFunction()
    {
        const std::size_t start = m_position;
        while (m_position < m_expression.size()
               && (std::isalnum(static_cast<unsigned char>(m_expression[m_position]))
                   || m_expression[m_position] == '_')) {
            ++m_position;
        }
        const std::string name = m_expression.substr(start, m_position - start);
        skipSpaces();
        if (m_position >= m_expression.size() || m_expression[m_position] != '(') {
            m_error = true;
            return 0.0;
        }

        ++m_position;
        const double firstArgument = parseExpression();
        skipSpaces();
        if (name == "pow") {
            if (m_position >= m_expression.size() || m_expression[m_position] != ',') {
                m_error = true;
                return 0.0;
            }
            ++m_position;
            const double secondArgument = parseExpression();
            skipSpaces();
            if (m_position >= m_expression.size() || m_expression[m_position] != ')') {
                m_error = true;
                return 0.0;
            }
            ++m_position;
            return std::pow(firstArgument, secondArgument);
        }

        if (m_position >= m_expression.size() || m_expression[m_position] != ')') {
            m_error = true;
            return 0.0;
        }
        ++m_position;
        if (name == "sqrt") {
            return std::sqrt(firstArgument);
        }
        if (name == "sqr" || name == "square") {
            return firstArgument * firstArgument;
        }

        m_error = true;
        return 0.0;
    }

    std::string m_expression;
    std::size_t m_position = 0;
    bool m_error = false;
};

} // namespace

bool evaluateSimpleExpression(const QString &expression, double *result)
{
    if (!result || expression.trimmed().isEmpty()) {
        return false;
    }

    SimpleExpressionParser parser(expression.toUtf8().constData());
    const double value = parser.parse();
    if (std::isnan(value) || std::isinf(value)) {
        return false;
    }

    *result = value;
    return true;
}

} // namespace configtool
