import PySouffle

datalog1 = ".type ObjectID \n .type Word \n \n .decl name(x: Word, y:ObjectID) \n name(\"apple\", \"1\"). \n name(\"banana\", \"2\"). \n .decl target(o:ObjectID)\n .output target\n target(O) :- name(\"banana\", O). \n .decl target1(o:ObjectID)\n .output target1\n target1(O) :- name(\"apple\", O)."
datalog2 = ".type ObjectID \n .type Word \n \n .decl name(x: Word, y:ObjectID) \n name(\"apple\", \"1\"). \n name(\"apple\", \"2\"). \n .decl target(o:ObjectID)\n .output target\n target(O) :- name(\"banana\", O). \n .decl target1(o:ObjectID)\n .output target1\n target1(O) :- name(\"apple\", O)."
datalog3 = ".type ObjectID \n .type Word \n \n .decl name(x: Word, y:ObjectID) \n name(\"banana\", \"1\"). \n name(\"banana\", \"2\"). \n .decl target(o:ObjectID)\n .output target\n target(O) :- name(\"banana\", O). \n .decl target1(o:ObjectID)\n .output target1\n target1(O) :- name(\"apple\", O)."

res1=PySouffle.execute(datalog1, False)
res2=PySouffle.execute(datalog2, False)
res3=PySouffle.execute(datalog3, False)

print(datalog1)
print ("end")