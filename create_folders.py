import os
import random

names = ['a','b','c','d','e','f','g','h']
paths = []
for i in range (1000):
    n = random.randint(1,10)
    path = "/home/student/Code/OS_HW4 - 2/OS_HW4/tests"
    for j in range (n):
        path+="/"
        path += random.choice(names)
    if (random.random() > 0.5):
        path += ".txt"
    paths.append(path)
paths = set(paths)
c = 0
for path in paths:
    try :
        os.makedirs (os.path.dirname(path))
        if ".txt" in path:
            c+=1
            with open(path,"w") as f:
                f.write(path)
                f.close()
    except:
        pass
print (len(paths), c)
    