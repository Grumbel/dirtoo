#!/usr/bin/env python3


import sys


def make_grammar():
    from pyparsing import Literal, Word, Forward, Optional, Group, alphas, alphanums, nums

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

    logical_or_expr  = Forward()
    logical_and_expr = Forward()
    bitwise_or_expr  = Forward()
    bitwise_xor_expr = Forward()
    bitwise_and_expr = Forward()
    equality_expr    = Forward()
    relational_expr  = Forward()
    shift_expr       = Forward()
    add_expr         = Forward()
    mult_expr        = Forward()
    unary_expr       = Forward()
                         
    logical_or_expr  <<= conditional_expr + Optional(logical_or    + conditional_expr)
    logical_and_expr <<= logical_or_expr  + Optional(logical_and   + logical_or_expr)
    bitwise_or_expr  <<= logical_and_expr + Optional(bitwise_or    + logical_and_expr)
    bitwise_xor_expr <<= bitwise_or_expr  + Optional(bitwise_xor   + bitwise_or_expr)
    bitwise_and_expr <<= bitwise_xor_expr + Optional(bitwise_and   + bitwise_xor_expr)
    equality_expr    <<= bitwise_and_expr + Optional(equal         + bitwise_and_expr)
    relational_expr  <<= equality_expr    + Optional(relational_op + equality_expr)
    shift_expr       <<= relational_expr  + Optional(shift         + relational_expr)
    add_expr         <<= shift_expr       + Optional(add_op        + shift_expr)
    mult_expr        <<= add_expr         + Optional(mul_op        + add_expr)
    unary_expr       <<= Optional(bitwise_not | logiclal_not | minus | plus) + mult_expr
    expr <<= unary_expr
    return expr


def main(argv):
    bnf = make_grammar()
    for arg in argv[1:]:
        print(arg)
        print(bnf.parseString(arg, parseAll=True))
        print()

if __name__ == "__main__":
    main(sys.argv)


# EOF #
