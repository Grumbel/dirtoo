#!/usr/bin/env python3

# dirtoo - File and directory manipulation tools for Python
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


from typing import cast, Any, Dict, Sequence, Callable, Optional, Union

import operator
from abc import ABC, abstractmethod
from pyparsing import ParserElement
from pyparsing.results import ParseResults
import pyparsing as pp


ExprValue = Union[int, float, bool, str]
ExprFunction = Callable[..., ExprValue]
UnaryExprFunction = Callable[[float], float]


# FIXME: these logical and/or operators don't have short circuit
# semantics, shouldn't matter since we try to stay side effect free,
# but might still be a worthy optimization
def op_logical_and(lhs: ExprValue, rhs: ExprValue) -> bool:
    return bool(lhs and rhs)


def op_logical_or(lhs: ExprValue, rhs: ExprValue) -> bool:
    return bool(lhs or rhs)


NAME2BINOP: Dict[str, ExprFunction] = {
    '+': operator.add,
    '-': operator.sub,
    '*': operator.mul,
    '**': operator.pow,
    '/': operator.truediv,
    '//': operator.floordiv,
    '%': operator.mod,
    '^': operator.xor,

    '<<': operator.lshift,
    '>>': operator.rshift,

    '&': operator.and_,
    '|': operator.or_,

    'and': op_logical_and,
    'or': op_logical_or,
    '&&': op_logical_and,
    '||': op_logical_or,

    '<=': operator.le,
    '>=': operator.ge,
    '<': operator.lt,
    '>': operator.gt,

    '=': operator.eq,
    '==': operator.eq,
    '!=': operator.ne,
}


NAME2UNARYOP: Dict[str, UnaryExprFunction] = {
    '+': lambda x: x,
    '-': operator.neg,
    '~': cast(UnaryExprFunction, operator.invert),
    '!': operator.not_,
    'not': operator.not_,
}


def get_operator_fn(op: str) -> ExprFunction:
    return NAME2BINOP[op]


def get_unary_operator_fn(op: str) -> UnaryExprFunction:
    return NAME2UNARYOP[op]


class Context:

    def __init__(self) -> None:
        self._functions: Dict[str, ExprFunction] = {
            'abs': cast(ExprFunction, abs),
            'int': cast(ExprFunction, int),
            'float': cast(ExprFunction, float),
        }

        self._variables: Dict[str, ExprValue] = {
            'version': "0.1",
            'true': True,
            'false': False,
            'True': True,
            'False': False,
        }

    def get_function(self, name: str) -> ExprFunction:
        return self._functions[name]

    def get_variable(self, name: str) -> ExprValue:
        return self._variables[name]

    def set_function(self, name: str, value: ExprFunction) -> None:
        self._functions[name] = value

    def set_variable(self, name: str, value: ExprValue) -> None:
        self._variables[name] = value


class Expr(ABC):

    @abstractmethod
    def eval(self, ctx: Context) -> ExprValue:
        pass


class Function(Expr):

    def __init__(self, s: str, loc: int, toks: ParseResults) -> None:
        self.name: str = cast(str, toks[0])
        self.args = toks[1:]

    def eval(self, ctx: Context) -> ExprValue:
        func = ctx.get_function(self.name)
        args = [arg.eval(ctx) for arg in self.args]
        return func(*args)

    def __repr__(self) -> str:
        return "Function({}, {})".format(self.name, ", ".join([repr(x) for x in self.args]))


class Number(Expr):

    def __init__(self, s: str, loc: int, toks: ParseResults) -> None:
        self.value: Union[int, float] = cast(Union[int, float], toks[0])
        self.unit: Optional[str]
        if len(toks) > 1:
            assert isinstance(toks[1], str)
            self.unit = cast(str, toks[1])
        else:
            self.unit = None

    def eval(self, ctx: Context) -> ExprValue:
        return self.value

    def __repr__(self) -> str:
        if self.unit is None:
            return str(self.value)
        else:
            return "{}[{}]".format(self.value, self.unit)


class String(Expr):

    def __init__(self, s: str, loc: int, toks: ParseResults) -> None:
        assert isinstance(toks[0], str)
        self.value: str = cast(str, toks[0])

    def eval(self, ctx: Context) -> ExprValue:
        return self.value

    def __repr__(self) -> str:
        return repr(self.value)


class Variable(Expr):

    def __init__(self, s: str, loc: int, toks: ParseResults) -> None:
        assert isinstance(toks[0], str)
        self.name: str = cast(str, toks[0])

    def eval(self, ctx: Context) -> ExprValue:
        return ctx.get_variable(self.name)

    def __repr__(self) -> str:
        return "Variable({})".format(self.name)


class Operator(Expr):

    def __init__(self, op: str, lhs: Expr, rhs: Expr) -> None:
        self.op = op
        self.lhs = lhs
        self.rhs = rhs

    def eval(self, ctx: Context) -> ExprValue:
        return get_operator_fn(self.op)(self.lhs.eval(ctx), self.rhs.eval(ctx))

    def __repr__(self) -> str:
        return "Operator({}, {}, {})".format(self.op, self.lhs, self.rhs)


class UnaryOperator(Expr):

    def __init__(self, op: str, lhs: Expr) -> None:
        self.op = op
        self.lhs = lhs

    def eval(self, ctx: Context) -> ExprValue:
        return get_unary_operator_fn(self.op)(cast(float, self.lhs.eval(ctx)))

    def __repr__(self) -> str:
        return "UnaryOperator({}, {})".format(self.op, self.lhs)


def make_grammar() -> ParserElement:
    ParserElement.enablePackrat()

    plus = pp.Literal("+")
    minus = pp.Literal("-")

    mul = pp.Literal("*")
    div = pp.Literal("/")
    floordiv = pp.Literal("//")
    mod = pp.Literal("%")

    lt = pp.Literal("<")
    le = pp.Literal("<=")

    gt = pp.Literal(">")
    ge = pp.Literal(">=")

    lshift = pp.Literal("<<")
    rshift = pp.Literal(">>")

    equal = pp.Literal("==") | pp.Literal("=") | pp.Literal("!=")

    bitwise_not = pp.Literal("~")
    bitwise_and = pp.Literal("&")
    bitwise_or = pp.Literal("|")
    bitwise_xor = pp.Literal("^")

    logical_not = pp.Literal("!") | pp.Keyword("not")
    logical_and = pp.Literal("&&") | pp.Literal("and") | pp.Keyword("AND")
    logical_or = pp.Literal("||") | pp.Keyword("or") | pp.Keyword("OR")

    ident = pp.Word(pp.alphas + "_", pp.alphanums + "_")
    functionname = pp.Word(pp.alphas + "_", pp.alphanums + "_")
    unit = pp.Word(pp.alphas)
    int_number = pp.Word(pp.nums)
    float_number = pp.Combine(pp.Word(pp.nums) + pp.Optional(pp.Literal(".") + pp.Word(pp.nums)))
    number = (float_number | int_number) + pp.Optional(unit)

    lparent = pp.Literal("(").suppress()
    rparent = pp.Literal(")").suppress()

    relational_op = (lt | le | gt | ge)
    shift = (lshift | rshift)
    add_op = (plus | minus)
    mul_op = (mul | floordiv | div | mod)

    expr = pp.Forward()
    string = (pp.QuotedString('"') | pp.QuotedString("'"))
    primary_expr = ident | number | string | (lparent + expr + rparent)

    def make_op(s: str, loc: int, toks: ParseResults) -> Any:
        if len(toks) == 1:
            return toks[0]
        else:
            def loop(lhs: Operator, rest: Sequence[ParserElement]) -> Operator:
                if len(rest) == 0:
                    return lhs
                else:
                    return loop(Operator(cast(str, rest[0]),
                                         lhs,
                                         cast(Expr, rest[1])),
                                rest[2:])

            return loop(Operator(toks[1], toks[0], toks[2]), toks[3:])

    def make_unary(s: str, loc: int, toks: ParseResults) -> Any:
        if len(toks) == 1:
            return toks[0]
        else:
            return UnaryOperator(toks[0], make_unary(s, loc, toks[1:]))

    argument_expression_list = expr + pp.ZeroOrMore(pp.Literal(",").suppress() + expr)
    function_expression = (functionname + lparent + argument_expression_list + rparent)
    postfix_expression = function_expression | primary_expr
    unary_expr = pp.ZeroOrMore(bitwise_not | logical_not | minus | plus) + postfix_expression
    cast_expresion = unary_expr | postfix_expression

    mult_expr        = cast_expresion   + pp.ZeroOrMore(mul_op        + cast_expresion)  # noqa: E221
    add_expr         = mult_expr        + pp.ZeroOrMore(add_op        + mult_expr)  # noqa: E221
    shift_expr       = add_expr         + pp.ZeroOrMore(shift         + add_expr)  # noqa: E221
    relational_expr  = shift_expr       + pp.ZeroOrMore(relational_op + shift_expr)  # noqa: E221
    equality_expr    = relational_expr  + pp.ZeroOrMore(equal         + relational_expr)  # noqa: E221
    bitwise_and_expr = equality_expr    + pp.ZeroOrMore(bitwise_and   + equality_expr)  # noqa: E221
    bitwise_xor_expr = bitwise_and_expr + pp.ZeroOrMore(bitwise_xor   + bitwise_and_expr)  # noqa: E221
    bitwise_or_expr  = bitwise_xor_expr + pp.ZeroOrMore(bitwise_or    + bitwise_xor_expr)  # noqa: E221
    logical_and_expr = bitwise_or_expr  + pp.ZeroOrMore(logical_and   + bitwise_or_expr)  # noqa: E221
    logical_or_expr  = logical_and_expr + pp.ZeroOrMore(logical_or    + logical_and_expr)  # noqa: E221
    expr <<= logical_or_expr

    function_expression.setParseAction(Function)

    int_number.setParseAction(lambda s, loc, toks: int(toks[0]))
    float_number.setParseAction(lambda s, loc, toks: float(toks[0]))
    number.setParseAction(Number)
    string.setParseAction(String)
    ident.setParseAction(Variable)

    unary_expr.setParseAction(make_unary)
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


class Parser:

    def __init__(self) -> None:
        self.bnf = make_grammar()

    def parse(self, text: str) -> Expr:
        result: ParseResults = self.bnf.parseString(text, parseAll=True)
        return cast(Expr, result[0])

    def eval(self, text: str, ctx: Context) -> ExprValue:
        result: ParseResults = self.bnf.parseString(text, parseAll=True)
        return cast(Expr, result[0]).eval(ctx)


# EOF #
