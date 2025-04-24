TARGETS = guesser2 guesserN2

all: $(TARGETS)

%: %.c
	g++ -Wall -Wextra -O2 $< -o $@

clean:
	rm -f $(TARGETS)
