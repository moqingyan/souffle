import PySouffle

datalog1 = ".type ObjectID \n .type Word \n \n .decl name(x: Word, y:ObjectID) \n name(\"apple\", \"1\"). \n name(\"banana\", \"2\"). \n .decl target(x: Word, y:ObjectID)\n .output target\n target(N, O) :- name(N, O). \n .decl target1(o:ObjectID)\n .output target1\n target1(O) :- name(\"apple\", O)."

# res1=PySouffle.execute(datalog1, True)
# res2=PySouffle.execute(datalog2, True)
# res3=PySouffle.execute(datalog3, True)
interpreter = PySouffle.Interpreter(datalog1)
rules = interpreter.execute()
# print(rules)

# for ruleName, ruleTuples in rules.items():
#     for ruleTuple in ruleTuples:
#         x = interpreter.explainRule(ruleName, ruleTuple)
x = interpreter.explainRulename("target")

# res3=PySouffle.execute(datalog3, True)

print("python:")
print(rules)
print(x)
print ("end")