declare i32 @nepoznata_funkcija()

define i32 @test5() {
entry:
%r = call i32 @nepoznata_funkcija()
%x = add i32 %r, 1
ret i32 %x
}