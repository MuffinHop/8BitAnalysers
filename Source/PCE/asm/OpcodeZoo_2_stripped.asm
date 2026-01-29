.cpu 6280
.list
.org $E000
EntryPoint:
BRK
ORA [$10,X]
SXY
ST0 #$10
TSB <$10
ORA <$10
ASL <$10
RMB0 <$10
PHP
ORA #$10
ASL
TSB data_FFF0
ORA data_FFF0
ASL data_FFF0
BBR0 <$10,EntryPoint
.label_10_1F:
BPL .label_10_1F
ORA [$10],Y
ORA data_FFF0
ST1 #$10
TRB <$10
ORA <$10,X
ASL <$10,X
RMB1 <$10
CLC
ORA data_FFF0,Y
INC
TRB data_FFF0
ORA data_FFF0,X
ASL data_FFF0,X
BBR1 <$10,.label_10_1F
.label_20_2f:
JSR data_FFF0
AND [$10,X]
SAX
ST2 #$10
BIT <$10
AND <$10
ROL <$10
RMB2 <$10
PLP
AND #$10
ROL
BIT data_FFF0
AND data_FFF0
ROL data_FFF0
BBR2 <$10,.label_20_2f
.label_30_3f:
BMI .label_30_3f
AND [$10],Y
AND [<$10]
BIT <$10,X
AND <$10,X
ROL <$10,X
RMB3 <$10
SEC
AND data_FFF0,Y
DEC
BIT data_FFF0,X
AND data_FFF0,X
ROL data_FFF0,X
BBR3 <$10,.label_30_3f
.label_40_4f:
RTI
EOR [$10,X]
SAY
TMA #$02
BSR .label_40_4f
EOR <$10
LSR <$10
RMB4 <$10
PHA
EOR #$10
LSR
JMP data_FFF0
EOR data_FFF0
LSR data_FFF0
BBR4 <$10,.label_40_4f
.label_50_5f:
BVC .label_50_5f
EOR [$10],Y
EOR [<$10]
TAM #$01
CSL
EOR <$10,X
LSR <$10,X
RMB5 <$10
CLI
EOR data_FFF0,Y
PHY
EOR data_FFF0,X
LSR data_FFF0,X
BBR5 <$10,.label_50_5f
.label_60_6f:
RTS
ADC [$10,X]
CLA
STZ <$10
ADC <$10
ROR <$10
RMB6 <$10
PLA
ADC #$10
ROR
JMP [data_FFF0]
ADC data_FFF0
ROR data_FFF0
BBR6 <$10,.label_60_6f
.label_70_7f:
BVS .label_70_7f
ADC [$10],Y
ADC [<$10]
TII $1000,$2000,$0010
STZ <$10,X
ADC <$10,X
ROR <$10,X
RMB7 <$10
SEI
ADC data_FFF0,Y
PLY
JMP [data_FFF0,X]
ADC data_FFF0,X
ROR data_FFF0,X
BBR7 <$10,.label_70_7f
.label_80_8f:
BRA .label_80_8f
STA [$10,X]
CLX
TST #$0F,<$10
STY <$10
STA <$10
STX <$10
SMB0 <$10
DEY
BIT #$10
TXA
STY data_FFF0
STA data_FFF0
STX data_FFF0
BBS0 <$10,.label_80_8f
.label_90_9f:
BCC .label_90_9f
STA [$10],Y
STA [<$10]
TST #$0F,data_FFF0
STY <$10,X
STA <$10,X
STX <$10,Y
SMB1 <$10
TYA
STA data_FFF0,Y
TXS
STZ data_FFF0
STA data_FFF0,X
STZ data_FFF0,X
BBS1 <$10,.label_90_9f
.label_a0_af:
LDY #$10
LDA [$10,X]
LDX #$10
TST #$10,<$10,X
LDY <$10
LDA <$10
LDX <$10
SMB2 <$10
TAY
LDA #$10
TAX
LDY data_FFF0
LDA data_FFF0
LDX data_FFF0
BBS2 <$10,.label_a0_af
.label_b0_bf:
BCS .label_b0_bf
LDA [$10],Y
LDA [<$10]
TST #$0F,data_FFF0,X
LDY <$10,X
LDA <$10,X
LDX <$10,Y
SMB3 <$10
CLV
LDA data_FFF0,Y
TSX
LDY data_FFF0,X
LDA data_FFF0,X
LDX data_FFF0,Y
BBS3 <$10,.label_b0_bf
.label_c0_cf:
CPY #$10
CMP [$10,X]
CLY
TDD $1000,$2000,$0010
CPY <$10
CMP <$10
DEC <$10
SMB4 <$10
INY
CMP #$10
DEX
CPY data_FFF0
CMP data_FFF0
DEC data_FFF0
BBS4 <$10,.label_c0_cf
.label_d0_df:
BNE .label_d0_df
CMP [$10],Y
CMP [<$10]
TIN $1000,$2000,$0010
CSH
CMP <$10,X
DEC <$10,X
SMB5 <$10
CLD
CMP data_FFF0,Y
PHX
CMP data_FFF0,X
DEC data_FFF0,X
BBS5 <$10,.label_d0_df
.label_e0_ef:
CPX #$10
SBC [$10,X]
TIA $1000,$2000,$0010
CPX <$10
SBC <$10
INC <$10
SMB6 <$10
INX
SBC #$10
NOP
CPX data_FFF0
SBC data_FFF0
INC data_FFF0
BBS6 <$10,.label_e0_ef
.label_f0_ff:
BEQ .label_f0_ff
SBC [$10],Y
SBC [<$10]
TAI $1000,$2000,$0010
SET
SBC <$10,X
INC <$10,X
SMB7 <$10
SED
SBC data_FFF0,Y
PLX
SBC data_FFF0,X
INC data_FFF0,X
BBS7 <$10,.label_f0_ff
.org $fff0
data_FFF0:
.org $fffe
.dw $e000
