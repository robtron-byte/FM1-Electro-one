# Prüft alle Bausteine (docs/adr/0040). Jeder Baustein hat sein eigenes Makefile mit dem Ziel `test`; dieses Makefile
# findet sie selbst, man muss es für einen neuen Baustein nicht ändern (parallele Chats).
#   make -C bausteine test     alle
#   make -C bausteine dk808    einer
BLOECKE := $(patsubst %/Makefile,%,$(wildcard */Makefile))

.PHONY: test liste $(BLOECKE)
test: $(BLOECKE)
	@echo "bausteine: $(words $(BLOECKE)) geprüft ($(BLOECKE))"

$(BLOECKE):
	$(MAKE) -C $@ test

liste:
	@echo $(BLOECKE)
