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
AND $1234       ; 2D 34 12
AND $1234,X     ;
AND $1234,Y     ;
AND [$10,X]     ;
AND [$10],Y     ;

ASL             ;
ASL <$10        ;
ASL $10         ;
ASL $10,X       ;
ASL $1234       ;
ASL $1234,X     ;

CMP #$80        ; C9 80
CMP <$10        ;
CMP $10         ;
CMP $10,X       ;
CMP $10,Y       ;
CMP $1234       ; CD 34 12
CMP $1234,X     ;
CMP $1234,Y     ;
CMP [$10,X]     ;
CMP [$10],Y     ;

CPX #$80        ; E0 80
CPX <$10        ;
CPX $10         ;
CPX $1234       ;

CPY #$80        ; C0 80
CPY <$10        ;
CPY $10         ;
CPY $1234       ;

DEC <$10         
DEC $10         
DEC $10,X       
DEC $1234       
DEC $1234,X     
DEX             
DEY             

EOR #$55            ;
EOR <$10            ; 
EOR $10             ; 45
EOR $10,X           ;
EOR $10,Y           ;
EOR $1234           ; 4D 34 12
EOR $1234,X         ;
EOR $1234,Y         ;
EOR [$10,X]         ;
EOR [$10],Y         ;

INC <$10            ;
INC $10             ;
INC $10,X           ;
INC $1234           ;
INC $1234,X         ;
INX                 ;
INY                 ;

LSR                 ;
LSR <$10            ;
LSR $10             ;
LSR $10,X           ;
LSR $1234           ;
LSR $1234,X         ;

ORA #$AA            ;
ORA <$10            ;
ORA $10             ; 05 10
ORA $10,X           ;
ORA $10,Y           ;
ORA $1234           ; 0D 34 12
ORA $1234,X         ;
ORA $1234,Y         ;
ORA [$10,X]         ;
ORA [$10],Y         ;

ROL                 
ROL <$10             
ROL $10             
ROL $10,X           
ROL $1234           
ROL $1234,X         

ROR                 
ROR <$10             
ROR $10             
ROR $10,X           
ROR $1234           
ROR $1234,X         

SBC #$11            
SBC <$10            ;
SBC $10             ; E5 10
SBC $10,X           
SBC $10,Y           
SBC $1234           ; ED 34 12
SBC $1234,X         
SBC $1234,Y         
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
LDA $1234           ; AD 34 12
LDA $1234,X         ; BD 34 12
LDA $1234,Y         ; B9 34 12
LDA [$10,X]         ; 
LDA [$10],Y         ; 

LDX #$02            ; A2 02
LDX <$10            ; 
LDX $10             ; 
LDX $10,Y           ; 
LDX $1234           ; 
LDX $1234,Y         ; 

LDY #$03            ; A0 03
LDY <$10            ; 
LDY $10             ; 
LDY $10,X           ; 
LDY $1234           ; 
LDY $1234,X         ; 
 
STA <$10            ; 
STA $10             ; 
STA $10,X           ; 
STA $10,Y           ; 
STA $1234           ; 8D 34 12
STA $1234,X         ; 9D 34 12
STA $1234,Y         ; 99 34 12
STA [$10,X]         ; 
STA [$10],Y         ; 

STX <$10            ; 
STX $10             ; 
STX $1234           ; 8E 34 12

STY <$10             ; 
STY $10             ; 
STY $1234           ; 8C 34 12

STZ <$10             ; 
STZ $10             ; 
STZ $10,X           ; 
STZ $1234           ; 9C 34 12
STZ $1234,X         ; 9E 34 12

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
BIT $1234               ; 2C 34 12

TRB <$10                 
TRB $10                 
TRB $1234               ; 1C 34 12

TSB <$10                 
TSB $10                 
TSB $1234               ; 0C 34 12

TST #$0F,$10            
TST #$0F,$10,X          
TST #$0F,$1234          
TST #$0F,$1234,X        

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



NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP


; ============================================================
; HuC6280 opcode reference
; Grouped by opcode page
; ============================================================

        .cpu 6280

; there should be 234 opcodes


; ------------------------------------------------------------
; 00–0F DONE
; ------------------------------------------------------------

label_00_0F:

BRK                         ; 00
ORA [$10,X]			        ; 01
SXY					        ; 02
ST0 #$10                    ; 03
TSB <$10			        ; 04
ORA <$10                    ; 05
ASL <$10                    ; 06
RMB0 <$10                   ; 07
PHP                         ; 08
ORA #$10                    ; 09
ASL A                       ; 0A
                            ; 0B missing
TSB $1234                   ; 0C
ORA $1234                   ; 0D
ASL $1234                   ; 0E
BBR0 <$10, label_00_0F      ; F

; ------------------------------------------------------------
; 10–1F DONE
; ------------------------------------------------------------

label_10_1F:

BPL label_10_1F             ; 10
ORA [$10],Y                 ; 11
ORA [<$10]                  ; 12
ST1 #$10                    ; 13
TRB <$10                    ; 14
ORA <$10,X                  ; 15
ASL <$10,X                  ; 16
RMB1 <$10                   ; 17
CLC                         ; 18
ORA $1234,Y                 ; 19
INC A                       ; 1A
                            ; 1B missing
TRB $1234                   ; 1C
ORA $1234,X                 ; 1D
ASL $1234,X                 ; 1E
BBR1 <$10, label_10_1F      ; 1F

; ------------------------------------------------------------
; 20–2F DONE
; ------------------------------------------------------------

label_20_2f:

JSR $1234                   ; 20
AND [$10,X]                 ; 21
SAX                         ; 22
ST2 #$10                    ; 23
BIT <$10                    ; 24
AND <$10                    ; 25
ROL <$10                    ; 26
RMB2 <$10                   ; 27
PLP                         ; 28
AND #$10                    ; 29
ROL A                       ; 2A
                            ; 2B missing
BIT $1234                   ; 2C
AND $1234                   ; 2D
ROL $1234                   ; 2E
BBR2 <$10, label_20_2f      ; 1F

; ------------------------------------------------------------
; 30–3F DONE
; ------------------------------------------------------------

label_30_3f:

BMI label_30_3f             ; 30
AND [$10],Y                 ; 31
AND [<$10]                  ; 32
                            ; 33 missing
BIT <$10,X                  ; 34
AND <$10,X                  ; 35
ROL <$10,X                  ; 36
RMB3 <$10                   ; 37
SEC                         ; 38
AND $1234,Y                 ; 39
DEC A                       ; 3A
                            ; 3B missing
BIT $1234,X                 ; 3C
AND $1234,X                 ; 3D
ROL $1234,X                 ; 3E
BBR3 <$10, label_30_3f      ; 3F

; ------------------------------------------------------------
; 40–4F DONE
; ------------------------------------------------------------

label_40_4f:

RTI                         ; 40
EOR [$10,X]                 ; 41
SAY                         ; 42
TMA #$02                    ; 43
BSR label_40_4f             ; 44
EOR <$10                    ; 45
LSR <$10                    ; 46
RMB4 <$10                   ; 47
PHA                         ; 48
EOR #$10                    ; 49
LSR A                       ; 4A
                            ; 4B missing
JMP $1234                   ; 4C
EOR $1234                   ; 4D
LSR $1234                   ; 4E
BBR4 <$10, label_40_4f      ; 4F

; ------------------------------------------------------------
; 50–5F DONE
; ------------------------------------------------------------

label_50_5f:

BVC label_50_5f             ; 50
EOR [$10],Y                 ; 51
EOR [<$10]                  ; 52
TAM #$01                    ; 53
CSL                         ; 54
EOR <$10,X                  ; 55
LSR <$10,X                  ; 56
RMB5 <$10                   ; 57
CLI                         ; 58
EOR $1234,Y                 ; 59
PHY                         ; 5A
                            ; 5B missing
                            ; 5C missing
EOR $1234,X                 ; 5D
LSR $1234,X                 ; 5E
BBR5 <$10, label_50_5f      ; 5F

; ------------------------------------------------------------
; 60–6F DONE
; ------------------------------------------------------------

label_60_6f:

RTS                         ; 60
ADC [$10,X]                 ; 61
CLA                         ; 62
                            ; 63 missing
STZ <$10                    ; 64
ADC <$10                    ; 65
ROR <$10                    ; 66
RMB6 <$10                   ; 67
PLA                         ; 68
ADC #$10                    ; 69
ROR A                       ; 6A
                            ; 6B missing
JMP [$1234]                 ; 6C
ADC $1234                   ; 6D
ROR $1234                   ; 6E
BBR6 <$10, label_60_6f      ; 6F

; ------------------------------------------------------------
; 70–7F DONE
; ------------------------------------------------------------

label_70_7f:

BVS label_70_7f             ; 70
ADC [$10],Y                 ; 71
ADC [<$10]                  ; 72
TII $1000,$2000,$0010       ; 73
STZ <$10,X                  ; 74
ADC <$10,X                  ; 75
ROR <$10,X                  ; 76
RMB7 <$10                   ; 77
SEI                         ; 78
ADC $1234,Y                 ; 79
PLY                         ; 7A
                            ; 7B missing
JMP [$1234,X]               ; 7C
ADC $1234,X                 ; 7D
ROR $1234,X                 ; 7E
BBR7 <$10, label_70_7f      ; 7F

; ------------------------------------------------------------
; 80–8F DONE
; ------------------------------------------------------------

label_80_8f:

BRA label_80_8f             ; 80
STA [$10,X]                 ; 81
CLX                         ; 82
TST #$0F,<$10               ; 83
STY <$10                    ; 84
STA <$10                    ; 85
STX <$10                    ; 86
SMB0 <$10                   ; 87
DEY                         ; 88
BIT #$10                    ; 89
TXA                         ; 8A
                            ; 8B missing
STY $1234                   ; 8C
STA $1234                   ; 8D
STX $1234                   ; 8E
BBS0 <$10, label_80_8f      ; 8F

; ------------------------------------------------------------
; 90–9F DONE
; ------------------------------------------------------------

label_90_9f:

BCC label_90_9f             ; 90
STA [$10],Y                 ; 91
STA [<$10]                  ; 92
TST #$0F,$1234              ; 93
STY <$10,X                  ; 94
STA <$10,X                  ; 95
STX <$10,Y                  ; 96
SMB1 <$10                   ; 97
TYA                         ; 98
STA $1234,Y                 ; 99
TXS                         ; 9A
                            ; 9B missing
STZ $1234                   ; 9C
STA $1234,X                 ; 9D
STZ $1234,X                 ; 9E
BBS1 <$10, label_90_9f      ; 9F

; ------------------------------------------------------------
; A0–AF DONE
; ------------------------------------------------------------

label_a0_af:

LDY #$10                    ; A0
LDA [$10,X]                 ; A1
LDX #$10                    ; A2
TST #$10,<$10,X             ; A3
LDY <$10                    ; A4
LDA <$10                    ; A5
LDX <$10                    ; A6
SMB2 <$10                   ; A7
TAY                         ; A8
LDA #$10                    ; A9
TAX                         ; AA
                            ; AB missing
LDY $1234                   ; AC
LDA $1234                   ; AD
LDX $1234                   ; AE

; ------------------------------------------------------------
; B0–BF
; ------------------------------------------------------------
label3:

BCS label3
LDA [$10],Y
LDY <$10,X
LDA <$10,X
LDX <$10,Y
CLV                 ; B8
LDA $1234,Y
TSX                 ; BA
LDY $1234,X
LDA $1234,X
LDX $1234,Y

; ------------------------------------------------------------
; C0–CF
; ------------------------------------------------------------

CPY #$10            ; C0 10
CMP [$10,X]
;CPY $10             ; C4 10
;CMP $10             ; C5 10
;DEC $10             ; C6 10
INY                 ; C8
CMP #$10            ; C9 10
DEX                 ; CA
CPY $1234           ; CC ll hh
CMP $1234           ; CD ll hh
DEC $1234           ; CE ll hh

; ------------------------------------------------------------
; D0–DF
; ------------------------------------------------------------

BNE label3
CMP [$10],Y
;CMP $10,X
;DEC $10,X
CLD                 ; D8
CMP $1234,Y
PHX                 ; DA
CMP $1234,X
DEC $1234,X

; ------------------------------------------------------------
; E0–EF
; ------------------------------------------------------------

CPX #$10            ; E0 10
SBC [$10,X]
;CPX $10             ; E4 10
;SBC $10             ; E5 10
;INC $10             ; E6 10
INX                 ; E8
SBC #$10            ; E9 10
NOP                 ; EA
CPX $1234           ; EC ll hh
SBC $1234           ; ED ll hh
INC $1234           ; EE ll hh

; ------------------------------------------------------------
; F0–FF
; ------------------------------------------------------------

BEQ label3
SBC [$10],Y
;SBC $10,X
;INC $10,X
SED                 ; F8
SBC $1234,Y
PLX                 ; FA
SBC $1234,X
INC $1234,X



	.org $fffe
	.dw $e000
;	.end