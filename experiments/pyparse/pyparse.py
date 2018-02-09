#!/usr/bin/env python3


import sys


def make_grammar():
    from pyparsing import (Literal, Word, Forward, Optional, Group,
                           ZeroOrMore, alphas, alphanums, nums)

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
    unit = Word(alphas)
    int_number = Word(nums)
    float_number = Group(Word(nums) + Optional(Literal(".") + Word(nums)))
    number = (float_number | int_number) + Optional(unit)

    lparent = Literal("(").suppress()
    rparent = Literal(")").suppress()

    relational_op = (lt | le | gt | ge)
    shift = (lshift | rshift)
    add_op = (plus | minus)
    mul_op = (mul | div)

    expr = Forward()
    primary_expr = ident | number | (lparent + expr + rparent)
    conditional_expr = primary_expr

    unary_expr       = ZeroOrMore(bitwise_not | logiclal_not | minus | plus) + primary_expr
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

    return expr


def main(argv):
    bnf = make_grammar()
    for arg in argv[1:]:
        print(arg)
        print(bnf.parseString(arg)) #, parseAll=True))
        print()

if __name__ == "__main__":
    main(sys.argv)


# EOF #
