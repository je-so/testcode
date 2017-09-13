
all: README

README:
	@echo "# Guessing Game Project" > README.md
	@echo "This project is part of a peer-graded assignment to learn Bash, Make, Git, and GitHub" >> README.md
	@echo "" >> README.md
	@echo "## Info" >> README.md
	@echo "The file \`guessinggame.sh\` contains" \
	$$(wc -l guessinggame.sh | egrep -o "[0-9]+" -) \
	"lines of code." >> README.md
	@echo "" >> README.md
	@echo "## Build" >> README.md
	@echo "The make command was run on *$$(LANG=C date -u)*." >> README.md
