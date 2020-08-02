import PySouffle

datalog = ".type ObjectID \n .type Word \n \n .decl name(x: Word, y:ObjectID) \n name(\"apple\", \"1\"). \n name(\"banana\", \"2\"). \n .decl target(o:ObjectID)\n .output target\n target(O) :- name(\"banana\", O)."
x=PySouffle.execute(datalog, False)
print(x)