	.file	"libtest.c"
	.text
	.weak	i			# A weak symbol,  that will get overridden in main
	.data
	.align 4
	.type	i, @object
	.size	i, 4
i:
	.long	2
