	.syntax	unified
	.cpu	cortex-m4
	.fpu	fpv4-sp-d16

	.global	main

	.text
	.align	2
	.thumb
	.thumb_func
main:
	movs	r0, #10
	ldr	r1, value2 /* r1 = 20 */ /* PC-relative adress [pc, #(value2-pc)]" */
	add	r0, r0, r1 /* r0 = 10 + 20 */
	bx	lr /* return 10 + 20 */
value2:
	.word 20
