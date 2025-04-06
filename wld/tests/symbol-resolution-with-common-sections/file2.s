.comm i, 4    # Two commons
.comm j, 4, 4 # Defined in file1

.section .data
.global k
.size k, 4
.align 4
k: .long 2
