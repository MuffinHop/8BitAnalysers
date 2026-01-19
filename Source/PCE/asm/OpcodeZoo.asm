; ============================================================
; HuC6280 Disassembler Unit test
; Strict Hudson-documented instruction set
; Includes explicit illegal opcode coverage
; Assembler: PCEAS 
; ============================================================

        .cpu 6280

; ------------------------------------------------------------
; Reset / Interrupt Vectors
; ------------------------------------------------------------

        .org $1FF0
        .dw reset      ; IRQ2
        .dw reset      ; IRQ1
        .dw reset      ; TIMER
        .dw reset      ; NMI
        .dw reset      ; RESET

; ------------------------------------------------------------
; Code starts here
; ------------------------------------------------------------

        .org $2000

reset:
        SEI
        CLD
        JMP test_start

; ------------------------------------------------------------
; ALU Instructions
; ------------------------------------------------------------

test_start:

ADC #$12
ADC $10
ADC $10,X
ADC $10,Y
ADC $1234
ADC $1234,X
ADC $1234,Y
ADC [$10,X]
ADC [$10],Y

AND #$34
AND $10
AND $10,X
AND $10,Y
AND $1234
AND $1234,X
AND $1234,Y
AND [$10,X]
AND [$10],Y

ASL
ASL $10
ASL $10,X
ASL $1234
ASL $1234,X

CMP #$80
CMP $10
CMP $10,X
CMP $10,Y
CMP $1234
CMP $1234,X
CMP $1234,Y
CMP [$10,X]
CMP [$10],Y

CPX #$80
CPX $10
CPX $1234

CPY #$80
CPY $10
CPY $1234

DEC $10
DEC $10,X
DEC $1234
DEC $1234,X
DEX
DEY

EOR #$55
EOR $10
EOR $10,X
EOR $10,Y
EOR $1234
EOR $1234,X
EOR $1234,Y
EOR [$10,X]
EOR [$10],Y

INC $10
INC $10,X
INC $1234
INC $1234,X
INX
INY

LSR
LSR $10
LSR $10,X
LSR $1234
LSR $1234,X

ORA #$AA
ORA $10
ORA $10,X
ORA $10,Y
ORA $1234
ORA $1234,X
ORA $1234,Y
ORA [$10,X]
ORA [$10],Y

ROL
ROL $10
ROL $10,X
ROL $1234
ROL $1234,X

ROR
ROR $10
ROR $10,X
ROR $1234
ROR $1234,X

SBC #$11
SBC $10
SBC $10,X
SBC $10,Y
SBC $1234
SBC $1234,X
SBC $1234,Y
SBC [$10,X]
SBC [$10],Y

; ------------------------------------------------------------
; Flag Instructions
; ------------------------------------------------------------

CLC
CLD
CLI
CLV
SEC
SED
SEI
SET

; ------------------------------------------------------------
; Data Transfer Instructions
; ------------------------------------------------------------

LDA #$01
LDA $10
LDA $10,X
LDA $10,Y
LDA $1234
LDA $1234,X
LDA $1234,Y
LDA [$10,X]
LDA [$10],Y

LDX #$02
LDX $10
LDX $10,Y
LDX $1234
LDX $1234,Y

LDY #$03
LDY $10
LDY $10,X
LDY $1234
LDY $1234,X

STA $10
STA $10,X
STA $10,Y
STA $1234
STA $1234,X
STA $1234,Y
STA [$10,X]
STA [$10],Y

STX $10
;STX $10,Y
STX $1234

STY $10
;STY $10,X
STY $1234

STZ $10
STZ $10,X
STZ $1234
STZ $1234,X

ST0 #$12
ST1 #$34
ST2 #$56

SAX
SAY
SXY

TAX
TAY
TSX
TXA
TXS
TYA

CLA
CLX
CLY

; ------------------------------------------------------------
; Branch Instructions
; ------------------------------------------------------------

BRA branch
BCC branch
BCS branch
BEQ branch
BMI branch
BNE branch
BPL branch
BVC branch
BVS branch

; Note: The < indicates zero page addressing
BBR0 <$10,branch
BBR7 <$10,branch
BBS0 <$10,branch
BBS7 <$10,branch

branch:

; ------------------------------------------------------------
; Subroutine / Stack Instructions
; ------------------------------------------------------------

JSR sub
RTS
RTI

PHA
PHP
PHX
PHY
PLA
PLP
PLX
PLY

sub:

; ------------------------------------------------------------
; Test Instructions
; ------------------------------------------------------------

BIT $10
BIT $1234

TRB $10
TRB $1234

TSB $10
TSB $1234

TST #$0F,$10
TST #$0F,$10,X
TST #$0F,$1234
TST #$0F,$1234,X

; ------------------------------------------------------------
; Control Instructions
; ------------------------------------------------------------

BRK
NOP

; Note: The < indicates zero page addressing
RMB0 <$10
RMB7 <$10
SMB0 <$10
SMB7 <$10

; ------------------------------------------------------------
; Block Transfer Instructions
; ------------------------------------------------------------

TII $1000,$2000,$0010
TIA $1000,$2000,$0010
TAI $1000,$2000,$0010
TIN $1000,$2000,$0010
TDD $1000,$2000,$0010

; ------------------------------------------------------------
; Mapping Register Instructions
; ------------------------------------------------------------

TAM #$01
TMA #$02

; ------------------------------------------------------------
; Explicit Illegal / Undefined Opcodes
; ------------------------------------------------------------

illegal_opcodes:
        .db $02
        .db $03
        .db $0B
        .db $1B
        .db $3B
        .db $5B
        .db $7B
        .db $9B
        .db $BB
        .db $DB
        .db $FB

hang:
        BRA hang

;        .end
