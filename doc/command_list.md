## MiniRDS command list

Most commands are in the ASCII format, for RT you can use `TEXT` or `RT1`, however some comamnds werent yet transitioned to the new command syntax, and here they are:

### Commands

#### `AF`
Ustawić AF:

`AF=98.6,95,89.1`

#### `AFCH`
Ustawić AF ale czestotliwośći są w hex  (98.6 = 6F) bo ([98.6 * 10] - 875)

`AFCH=6F,6A`

#### `DI`
Ustawić DI

`DI=1`

`DI=9`
#### `MS`
Ustawić MS

`MS=1`
#### `PI`
Ustawić PI

`PI=30FE`

#### `PS`
Ustawić PS

`PS= R-AFERA`

#### `TPS`
Ustawić TPS (Traffic PS jest emitowny kiedy TA jest włączone)

`TPS=DROGA`

#### `PTY`
Ustawić PTY

`PTY=11`

#### `PTYN`
Ustawić PTYN

`PTYN=Alternatywny`

#### `PTYNEN`
Właczyć/wyłaczyć PTYN

`PTYN=1`

#### `TEXT`
Ustawić RT

`RT=Gramy: Artic Monkeys - 505`

#### `RT1EN`
Właczyć RT

#### `TA`
`TA=1`

#### `TP`
`TP=0`

#### `CT`
Właczyć CT
`CT=1`

#### `RDSGEN`
Tryby kodera:

0 - Brak RDS+RDS2
1 - RDS1
2 - RDS1+RDS2

#### `ECC`
`ECC=E2`

#### `ECCEN`
`ECCEN=1`

#### `G`
Wewnętrzna grupa
`G=5000FFFFFFFF`

#### `LIC`
`LIC=09`

#### `RTP`
Tagi RTP
w kolejnośći:
Typ
Start
Dlugosc
 x2
`RTP=30,8,8,31,16,16`

#### `RTPRUN`
Czy RTP jest teraz aktywne?
`RTPRUN=1`

## Other
See src/supported_cmds.txt