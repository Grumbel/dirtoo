#!/usr/bin/env python3


import sys
import operator


class Function:

    def __init__(self, s, loc, toks):
        self.name = toks[0]
        self.args = toks[1:]

    def __repr__(self):
        return "Function({}, {})".format(self.name, ", ".join([repr(x) for x in self.args]))


class Number:

    def __init__(self, s, loc, toks):
        self.value = toks[0]
        if len(toks) > 1:
            self.unit = toks[1]
        else:
            self.unit = None

    def __repr__(self):
        if self.unit is None:
            return str(self.value)
        else:
            return "{}{}".format(self.value, self.unit)


class Variable:

    def __init__(self, s, loc, toks):
        self.name = toks[0]

    def __repr__(self):
        return "Variable({})".format(self.name)


class Operator:

    def __init__(self, op, lhs, rhs):
        self.op = op
        self.lhs = lhs
        self.rhs = rhs

    def __repr__(self):
        return "Operator({}, {}, {})".format(self.op, self.lhs, self.rhs)


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

    lt = Literal("<")
    le = Literal("<=")

    gt = Literal(">")
    ge = Literal(">=")

    lshift = Literal("<<")
    rshift = Literal(">>")

    equal = Literal("==") | Literal("=")

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
    mul_op = (mul | div)

    expr = Forward()
    string = (QuotedString('"') | QuotedString("'"))
    primary_expr = ident | number | string | (lparent + expr + rparent)
    conditional_expr = primary_expr

    def make_op(s, loc, toks):
        if len(toks) == 1:
            return toks[0]
        else:
            return [Operator(toks[1], toks[0], toks[2])] +  toks[3:]

    argument_expression_list = expr + ZeroOrMore(Literal(",").suppress() + expr)
    postfix_expression = functionname + lparent + argument_expression_list + rparent
    unary_expr       = postfix_expression | (ZeroOrMore(bitwise_not | logiclal_not | minus | plus) + primary_expr)
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

    postfix_expression.setParseAction(Function)
    number.setParseAction(Number)
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

    expr <<= logical_or_expr
    return expr


def main(argv):
    bnf = make_grammar()
    for arg in argv[1:]:
        print("Input:", arg)
        print("Result:", bnf.parseString(arg, parseAll=True))
        print()

if __name__ == "__main__":
    main(sys.argv)


# EOF #
