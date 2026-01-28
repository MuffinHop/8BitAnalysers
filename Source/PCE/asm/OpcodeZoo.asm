; ============================================================
; HuC6280 Disassembler Unit test
; Strict Hudson-documented instruction set
; Includes explicit illegal opcode coverage
; Assembler: PCEAS 
; ============================================================

	.cpu 6280

	.list

; ------------------------------------------------------------
; Code starts here
; ------------------------------------------------------------

	.org $E000

	SEI
	CLD

; ------------------------------------------------------------
; ALU Instructions
; ------------------------------------------------------------

start:
JMP start

ADC #$12        ; 69 12
ADC <$10        ;
ADC $10         ;
ADC start       ; 6D
ADC start,X     ; 7D
ADC start,Y     ; 79
ADC [$10,X]     ; 61
ADC [$10],Y     ; 71

AND #$34        ; 29 34
AND <$10        ;
AND $10         ;
AND $10,X       ;
AND $10,Y       ;
AND label_fff0       ; 2D 34 12
AND label_fff0,X     ;
AND label_fff0,Y     ;
AND [$10,X]     ;
AND [$10],Y     ;

ASL             ;
ASL <$10        ;
ASL $10         ;
ASL $10,X       ;
ASL label_fff0       ;
ASL label_fff0,X     ;

CMP #$80        ; C9 80
CMP <$10        ;
CMP $10         ;
CMP $10,X       ;
CMP $10,Y       ;
CMP label_fff0       ; CD 34 12
CMP label_fff0,X     ;
CMP label_fff0,Y     ;
CMP [$10,X]     ;
CMP [$10],Y     ;

CPX #$80        ; E0 80
CPX <$10        ;
CPX $10         ;
CPX label_fff0       ;

CPY #$80        ; C0 80
CPY <$10        ;
CPY $10         ;
CPY label_fff0       ;

DEC <$10         
DEC $10         
DEC $10,X       
DEC label_fff0       
DEC label_fff0,X     
DEX             
DEY             

EOR #$55            ;
EOR <$10            ; 
EOR $10             ; 45
EOR $10,X           ;
EOR $10,Y           ;
EOR label_fff0           ; 4D 34 12
EOR label_fff0,X         ;
EOR label_fff0,Y         ;
EOR [$10,X]         ;
EOR [$10],Y         ;

INC <$10            ;
INC $10             ;
INC $10,X           ;
INC label_fff0           ;
INC label_fff0,X         ;
INX                 ;
INY                 ;

LSR                 ;
LSR <$10            ;
LSR $10             ;
LSR $10,X           ;
LSR label_fff0           ;
LSR label_fff0,X         ;

ORA #$AA            ;
ORA <$10            ;
ORA $10             ; 05 10
ORA $10,X           ;
ORA $10,Y           ;
ORA label_fff0           ; 0D 34 12
ORA label_fff0,X         ;
ORA label_fff0,Y         ;
ORA [$10,X]         ;
ORA [$10],Y         ;

ROL                 
ROL <$10             
ROL $10             
ROL $10,X           
ROL label_fff0           
ROL label_fff0,X         

ROR                 
ROR <$10             
ROR $10             
ROR $10,X           
ROR label_fff0           
ROR label_fff0,X         

SBC #$11            
SBC <$10            ;
SBC $10             ; E5 10
SBC $10,X           
SBC $10,Y           
SBC label_fff0           ; ED 34 12
SBC label_fff0,X         
SBC label_fff0,Y         
SBC [$10,X]             
SBC [$10],Y         

; ------------------------------------------------------------
; Flag Instructions
; ------------------------------------------------------------

CLD     ; D8
CLC     ; 18
CLI     ; 58
CLV     ; B8
SEC     ; 38
SED     ; F8

SEI     ; 78
SET

; ------------------------------------------------------------
; Data Transfer Instructions
; ------------------------------------------------------------

LDA #$01            ; A9 01
LDA <$10            ; 
LDA $10             ; 
LDA $10,X           ; 
LDA $10,Y           ; 
LDA label_fff0           ; AD 34 12
LDA label_fff0,X         ; BD 34 12
LDA label_fff0,Y         ; B9 34 12
LDA [$10,X]         ; 
LDA [$10],Y         ; 

LDX #$02            ; A2 02
LDX <$10            ; 
LDX $10             ; 
LDX $10,Y           ; 
LDX label_fff0           ; 
LDX label_fff0,Y         ; 

LDY #$03            ; A0 03
LDY <$10            ; 
LDY $10             ; 
LDY $10,X           ; 
LDY label_fff0           ; 
LDY label_fff0,X         ; 
 
STA <$10            ; 
STA $10             ; 
STA $10,X           ; 
STA $10,Y           ; 
STA label_fff0           ; 8D 34 12
STA label_fff0,X         ; 9D 34 12
STA label_fff0,Y         ; 99 34 12
STA [$10,X]         ; 
STA [$10],Y         ; 

STX <$10            ; 
STX $10             ; 
STX label_fff0           ; 8E 34 12

STY <$10             ; 
STY $10             ; 
STY label_fff0           ; 8C 34 12

STZ <$10             ; 
STZ $10             ; 
STZ $10,X           ; 
STZ label_fff0           ; 9C 34 12
STZ label_fff0,X         ; 9E 34 12

ST0 #$12            ; 03 12
ST1 #$34            ; 13 34
ST2 #$56            ; 23 56

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
BBR1 <$10,branch
BBR2 <$10,branch
BBR3 <$10,branch
BBR4 <$10,branch
BBR5 <$10,branch
BBR6 <$10,branch
BBR7 <$10,branch
BBS0 <$10,branch
BBS1 <$10,branch
BBS2 <$10,branch
BBS3 <$10,branch
BBS4 <$10,branch
BBS5 <$10,branch
BBS6 <$10,branch
BBS7 <$10,branch

branch:

; ------------------------------------------------------------
; Subroutine / Stack Instructions
; ------------------------------------------------------------

JSR sub
RTS
RTI

PHA                 ; 48
PHP                 ; 08
PHX                 ; DA
PHY                 ; 5A
PLA                 ; 68
PLP                 ; 28
PLX                 ; 
PLY                 ; 7A

sub:

; ------------------------------------------------------------
; Test Instructions
; ------------------------------------------------------------

BIT <$10                 
BIT $10                 
BIT label_fff0               ; 2C 34 12

TRB <$10                 
TRB $10                 
TRB label_fff0               ; 1C 34 12

TSB <$10                 
TSB $10                 
TSB label_fff0               ; 0C 34 12

TST #$0F,$10            
TST #$0F,$10,X          
TST #$0F,label_fff0          
TST #$0F,label_fff0,X        

; ------------------------------------------------------------
; Control Instructions
; ------------------------------------------------------------

BRK                     ; 00
NOP                     ; EA

; Note: The < indicates zero page addressing
RMB0 <$10
RMB1 <$10
RMB2 <$10
RMB3 <$10
RMB4 <$10
RMB5 <$10
RMB6 <$10
RMB7 <$10
SMB0 <$10
SMB1 <$10
SMB2 <$10
SMB3 <$10
SMB4 <$10
SMB5 <$10
SMB6 <$10
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

TAM #$01            ; 53 01
TMA #$02            ; 43 02

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

    .org $fff0
    label_fff0:

	.org $fffe
	.dw $e000
;	.end