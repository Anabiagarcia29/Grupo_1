# Configurações básicas do sistema
.include <bsd.own.mk>

# Nome do programa
PROG=   teste_processos

# Arquivos fonte
SRCS=   teste_processos.c

# Diretório de instalação (opcional)
BINDIR?= /usr/tests

# Configurações de compilação
NOGCCERROR:= yes  # Ignora erros do GCC
WARNS?= 1         # Nível de warnings
CFLAGS+= -O2      # Otimização
LDADD+= -lm       # Link com biblioteca matemática (se necessário)

# Não gerar manpages
MKMAN= no

# Permissões
BINOWN= bin
BINMODE= 555

# Inclui as regras de compilação
.include <bsd.prog.mk>
