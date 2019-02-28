all:
	
thread:
	g++ -c -o threads.o threads.cpp -m32

test:
	g++ -o $(name)  tests/$(name).c threads.o -m32

join:
	g++ -o join_1 mytests/join_1.c threads.o -m32
	g++ -o join_2 mytests/join_2.c threads.o -m32
	g++ -o join_3 mytests/join_3.c threads.o -m32
	g++ -o join_4 mytests/join_4.c threads.o -m32
	g++ -o join_5 mytests/join_5.c threads.o -m32
	g++ -o join_6 mytests/join_6.c threads.o -m32

sem:
	g++ -o sem_1 mytests/sem_1.c threads.o -m32
	
basic:
	g++ -o basic_sem_1 proj3_tests/tests/basic_sem_1.c threads.o -m32
	g++ -o basic_sem_2 proj3_tests/tests/basic_sem_2.c threads.o -m32
	g++ -o basic_sem_3 proj3_tests/tests/basic_sem_3.c threads.o -m32
	g++ -o basic_sem_4 proj3_tests/tests/basic_sem_4.c threads.o -m32

clean_thread:
	rm threads.o

clean_join:
	rm join_1
	rm join_2
	rm join_3
	rm join_4
	rm join_5
	rm join_6

clean_sem:
	rm sem_1

clean_basic:
	rm basic_sem_1
	rm basic_sem_2
	rm basic_sem_3
	rm basic_sem_4

both: thread test

clean:
	rm threads.o
	rm $(name)

clean_test:
	rm $(name)
