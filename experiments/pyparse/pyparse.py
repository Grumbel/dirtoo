#!/usr/bin/env python3

# dirtool.py - diff tool for directories
# Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


import sys
import operator


def logical_and(lhs, rhs):
    return lhs and rhs


def logical_or(lhs, rhs):
    return lhs or rhs


def get_operator_fn(op):
    return {
        '+' : operator.add,
        '-' : operator.sub,
        '*' : operator.mul,
        '**' : operator.pow,
        '/' : operator.truediv,
        '//' : operator.floordiv,
        '%' : operator.mod,
        '^' : operator.xor,

        '<<': operator.lshift,
        '>>': operator.rshift,

        '&': operator.and_,
        '|': operator.or_,

        'and': logical_and,
        'or': logical_or,
        '&&': logical_and,
        '||': logical_or,

        '<=': operator.le,
        '>=': operator.ge,
        '<': operator.lt,
        '>': operator.gt,

        '=': operator.eq,
        '==': operator.eq,
        '!=': operator.ne,

        }[op]


def get_unary_operator_fn(op):
    return {
        '+': lambda x: x,
        '-': operator.neg,
        '~': operator.invert,
        '!': operator.not_,
        'not': operator.not_,
    }[op]


class Context:

    def __init__(self):
        self._functions = {
            'abs': abs
        }

        self._variables = {
            'version': "0.1",
            'true': True,
            'false': False,
            'True': True,
            'False': False,
        }

    def get_function(self, name):
        return self._functions[name]

    def get_variable(self, name):
        return self._variables[name]


class Function:

    def __init__(self, s, loc, toks):
        self.name = toks[0]
        self.args = toks[1:]


    def eval(self, ctx):
        return ctx.get_function(self.name)(*[arg.eval(ctx) for arg in self.args])

    def __repr__(self):
        return "Function({}, {})".format(self.name, ", ".join([repr(x) for x in self.args]))


class Number:

    def __init__(self, s, loc, toks):
        self.value = toks[0]
        if len(toks) > 1:
            self.unit = toks[1]
        else:
            self.unit = None

    def eval(self, ctx):
        return int(self.value)

    def __repr__(self):
        if self.unit is None:
            return str(self.value)
        else:
            return "{}[{}]".format(self.value, self.unit)


class String:

    def __init__(self, s, loc, toks):
        self.value = toks[0]

    def eval(self, ctx):
        return self.value

    def __repr__(self):
        return repr(self.value)


class Variable:

    def __init__(self, s, loc, toks):
        self.name = toks[0]

    def eval(self, ctx):
        return ctx.get_variable(self.name)

    def __repr__(self):
        return "Variable({})".format(self.name)


class Operator:

    def __init__(self, op, lhs, rhs):
        self.op = op
        self.lhs = lhs
        self.rhs = rhs

    def eval(self, ctx):
        return get_operator_fn(self.op)(self.lhs.eval(ctx), self.rhs.eval(ctx))

    def __repr__(self):
        return "Operator({}, {}, {})".format(self.op, self.lhs, self.rhs)


class UnaryOperator:

    def __init__(self, op, lhs):
        self.op = op
        self.lhs = lhs

    def eval(self, ctx):
        return get_unary_operator_fn(self.op)(self.lhs.eval(ctx))

    def __repr__(self):
        return "UnaryOperator({}, {})".format(self.op, self.lhs)


def make_grammar():
    from pyparsing import (Literal, Word, Forward, Optional, Group,
                           QuotedString, Combine, ZeroOrMore, Suppress,
                           alphas, alphanums, nums)

    def test(s, loc, toks):
        print()
        print("'{}' {} =>\n  {}".format(s, loc, toks))
        print()
        return None

    plus = Literal("+")
    minus = Literal("-")

    mul = Literal("*")
    div = Literal("/")
    floordiv = Literal("//")
    mod = Literal("%")

    lt = Literal("<")
    le = Literal("<=")

    gt = Literal(">")
    ge = Literal(">=")

    lshift = Literal("<<")
    rshift = Literal(">>")

    equal = Literal("==") | Literal("=") | Literal("!=")

    bitwise_not = Literal("~")
    bitwise_and = Literal("&")
    bitwise_or = Literal("|")
    bitwise_xor = Literal("^")

    logiclal_not = Literal("!") | Literal("not")
    logical_and = Literal("&&") | Literal("and") | Literal("AND")
    logical_or = Literal("||") | Literal("or") | Literal("OR")

    ident = Word(alphas + "_", alphanums + "_")
    functionname = Word(alphas + "_", alphanums + "_")
    unit = Word(alphas)
    int_number = Word(nums)
    float_number = Combine(Word(nums) + Optional(Literal(".") + Word(nums)))
    number = (float_number | int_number) + Optional(unit)

    lparent = Literal("(").suppress()
    rparent = Literal(")").suppress()

    relational_op = (lt | le | gt | ge)
    shift = (lshift | rshift)
    add_op = (plus | minus)
    mul_op = (mul | floordiv | div | mod)

    expr = Forward()
    string = (QuotedString('"') | QuotedString("'"))
    primary_expr = ident | number | string | (lparent + expr + rparent)

    def make_op(s, loc, toks):
        if len(toks) == 1:
            return toks[0]
        else:
            return [Operator(toks[1], toks[0], toks[2])] +  toks[3:]

    def make_unary(s, loc, toks):
        if len(toks) == 1:
            return toks[0]
        else:
            return UnaryOperator(toks[0], make_unary(s, loc, toks[1:]))

    argument_expression_list = expr + ZeroOrMore(Literal(",").suppress() + expr)
    function_expression = (functionname + lparent + argument_expression_list + rparent)
    postfix_expression = function_expression | primary_expr
    unary_expr       = postfix_expression | (ZeroOrMore(bitwise_not | logiclal_not | minus | plus) + postfix_expression).setParseAction(make_unary)

    mult_expr        = unary_expr       + ZeroOrMore(mul_op        + unary_expr)
    add_expr         = mult_expr        + ZeroOrMore(add_op        + mult_expr)
    shift_expr       = add_expr         + ZeroOrMore(shift         + add_expr)
    relational_expr  = shift_expr       + ZeroOrMore(relational_op + shift_expr)
    equality_expr    = relational_expr  + ZeroOrMore(equal         + relational_expr)
    bitwise_and_expr = equality_expr    + ZeroOrMore(bitwise_and   + equality_expr)
    bitwise_xor_expr = bitwise_and_expr + ZeroOrMore(bitwise_xor   + bitwise_and_expr)
    bitwise_or_expr  = bitwise_xor_expr + ZeroOrMore(bitwise_or    + bitwise_xor_expr)
    logical_and_expr = bitwise_or_expr  + ZeroOrMore(logical_and   + bitwise_or_expr)
    logical_or_expr  = logical_and_expr + ZeroOrMore(logical_or    + logical_and_expr)
    expr <<= logical_or_expr

    function_expression.setParseAction(Function)
    number.setParseAction(Number)
    string.setParseAction(String)
    ident.setParseAction(Variable)

    mult_expr.setParseAction(make_op)
    add_expr.setParseAction(make_op)
    shift_expr.setParseAction(make_op)
    relational_expr.setParseAction(make_op)
    equality_expr.setParseAction(make_op)
    bitwise_and_expr.setParseAction(make_op)
    bitwise_xor_expr.setParseAction(make_op)
    bitwise_or_expr.setParseAction(make_op)
    logical_and_expr.setParseAction(make_op)
    logical_or_expr.setParseAction(make_op)

    return expr


def main(argv):
    bnf = make_grammar()
    for arg in argv[1:]:
        print("Input:", arg)
        expr = bnf.parseString(arg, parseAll=True)[0]
        print("Result:", expr)
        ctx = Context()
        print("Eval:", expr.eval(ctx))
        print()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
