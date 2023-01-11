import random

i = 0
j = 0
A = 0
B = 0
counter = 0
gennum = None
answer = None
guess = None

from datetime import datetime
random.seed(datetime.now().timestamp())
gennum = [random.random() for _ in range(4)]
print(gennum)
print(type(gennum))
