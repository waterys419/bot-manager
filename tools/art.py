"""Original indexed pixel art; PNG sources -> SNES planar 4bpp. No dependencies."""
from pathlib import Path
import struct,zlib
ROOT=Path(__file__).resolve().parents[1]
PAL=['101329','242844','66658c','eee6c4','ffc46b','ed7766','579fc2','7bdbca','9db87a','865e68','c6a28a','394769','50314e','fff9e9','bd485d','b695cd']
FONT={
'A':'0E11111F111111','B':'1E11111E11111E','C':'0F10101010100F','D':'1E11111111111E','E':'1F10101E10101F','F':'1F10101E101010','G':'0F10101311110F','H':'1111111F111111','I':'0E04040404040E','J':'0702020212120C','K':'11121418141211','L':'1010101010101F','M':'111B1515111111','N':'11191513111111','O':'0E11111111110E','P':'1E11111E101010','Q':'0E11111115120D','R':'1E11111E141211','S':'0F10100E01011E','T':'1F040404040404','U':'1111111111110E','V':'11111111110A04','W':'11111115151B11','X':'11110A040A1111','Y':'11110A04040404','Z':'1F01020408101F',
'0':'0E11131519110E','1':'040C040404040E','2':'0E11010204081F','3':'1E01010601011E','4':'02060A121F0202','5':'1F101E0101110E','6':'0E10101E11110E','7':'1F010204080808','8':'0E11110E11110E','9':'0E11110F01010E',
'.':'00000000000C0C',',':'000000000C0C08',':':'000C0C000C0C00',';':'000C0C000C0C08','!':'04040404040004','?':'0E110102040004','-':'0000001F000000','+':'0004041F040400','/':'01010204081010','>':'10080402040810','<':'01020408040201','=':'00001F001F0000','(':'02040808080402',')':'08040202020408',"'":'04040400000000','%':'19190204081313','_':'0000000000001F','[':'0E08080808080E',']':'0E02020202020E','*':'00150E1F0E1500','#':'0A1F0A0A1F0A00','|':'04040404040404',' ':'00000000000000'}

def png(path,w,h,data):
 def chunk(tag,b):return struct.pack('>I',len(b))+tag+b+struct.pack('>I',zlib.crc32(tag+b)&0xffffffff)
 palette=b''.join(bytes.fromhex(c) for c in PAL)
 raw=b''.join(b'\0'+bytes(data[y*w:(y+1)*w]) for y in range(h))
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,3,0,0,0))+chunk(b'PLTE',palette)+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

def convert(path,out):
 data=path.read_bytes();p=8;compressed=b'';palette=b''
 while p<len(data):
  n=int.from_bytes(data[p:p+4],'big');tag=data[p+4:p+8];b=data[p+8:p+8+n];p+=n+12
  if tag==b'IHDR':w,h,depth,kind,*_=struct.unpack('>IIBBBBB',b);assert(depth,kind)==(8,3)
  if tag==b'IDAT':compressed+=b
  if tag==b'PLTE':palette=b
 raw=zlib.decompress(compressed);pixels=[]
 for y in range(h):assert raw[y*(w+1)]==0;pixels.extend(raw[y*(w+1)+1:(y+1)*(w+1)])
 tiles=bytearray()
 for ty in range(h//8):
  for tx in range(w//8):
   for pair in (0,2):
    for y in range(8):
     for bit in (pair,pair+1):
      tiles.append(sum(((pixels[(ty*8+y)*w+tx*8+x]>>bit)&1)<<(7-x) for x in range(8)))
 out.with_suffix('.pic').write_bytes(tiles)
 out.with_suffix('.pal').write_bytes(b''.join(struct.pack('<H',(palette[i]>>3)|((palette[i+1]>>3)<<5)|((palette[i+2]>>3)<<10)) for i in range(0,len(palette),3)))

class Art:
 def __init__(self,w,h):self.w=w;self.h=h;self.p=[0]*(w*h)
 def rect(self,x,y,w,h,c):
  for yy in range(max(0,y),min(self.h,y+h)):
   for xx in range(max(0,x),min(self.w,x+w)):self.p[yy*self.w+xx]=c
 def dot(self,x,y,c):self.rect(x,y,1,1,c)
 def tile(self,t,rows):
  x=t%16*8;y=t//16*8
  for yy,row in enumerate(rows):
   for xx,v in enumerate(row):self.dot(x+xx,y+yy,int(v,16))
 def save(self,name):
  path=ROOT/'src/art'/f'{name}.png';png(path,self.w,self.h,self.p);convert(path,ROOT/'build'/name)

def main():
 a=Art(128,256)
 for ch,bits in FONT.items():
  t=ord(ch);x=t%16*8;y=t//16*8
  for yy in range(7):
   v=int(bits[yy*2:yy*2+2],16)
   for xx in range(5):
    if v&(16>>xx):a.dot(x+xx+1,y+yy,13)
 # Solid colors / shaded borders / decor, tiles 128+.
 for c in range(16):a.rect(c*8,64,8,8,c)
 a.tile(144,['bbbbbbbb','b666666b','b677776b','b677776b','b677776b','b666666b','bbbbbbbb','00000000'])
 a.tile(145,['bbbbbbbb','b444444b','b433334b','b433334b','b444444b','bbbbbbbb','bbbbbbbb','bbbbbbbb'])
 a.tile(146,['00000000','000d0000','000d0000','0ddddd00','000d0000','000d0000','00000000','00000000'])
 a.tile(147,['bbb99999','bbaaaaa9','bb333339','bb333339','bbaaaaa9','bb999999','bb000000','00000000'])
 a.tile(148,['99999999','aaaaaaaa','a333333a','aaaaaaaa','99999999','00000000','00000000','00000000'])
 a.tile(149,['00000000','00ee0ee0','0eeeeeee','0eeeeeee','00eeeee0','000eee00','0000e000','00000000'])
 a.tile(150,['00000000','00111100','01666610','16777761','16777761','01666610','00111100','00000000'])
 a.tile(151,['00000000','00044000','00444400','04444440','04444440','00444400','00044000','00000000'])
 a.tile(152,['55555555','eeeeeeee','eeeeeeee','eeeeeeee','eeeeeeee','eeeeeeee','55555555','11111111'])
 a.tile(153,['77777777','66666666','66666666','66666666','66666666','66666666','77777777','11111111'])
 a.tile(154,['44444444','99999999','33333333','33333333','33333333','99999999','44444444','11111111'])
 a.tile(155,['88888888','33333333','33333333','88888888','88888888','88888888','88888888','11111111'])
 a.tile(156,['00000000','00011000','00111100','00111100','00011000','00011000','00110000','00000000'])
 a.tile(157,['b1b1b1b1','1b1b1b1b','b1b1b1b1','1b1b1b1b','b1b1b1b1','1b1b1b1b','b1b1b1b1','1b1b1b1b'])
 a.tile(158,['77777777','71111111','71111111','71111111','71111111','71111111','71111111','71111111'])
 a.tile(159,['11111117','11111117','11111117','11111117','11111117','11111117','11111117','77777777'])
 # Hand-built large title logo, stored as a 32x6 tile strip (tiles 160..351).
 logo=Art(256,48)
 for i,ch in enumerate('BOT MANAGER'):
  bits=FONT[ch]
  for yy in range(7):
   v=int(bits[yy*2:yy*2+2],16)
   for xx in range(5):
    if v&(16>>xx):
     x=29+i*18+xx*3;y=7+yy*3
     logo.rect(x+2,y+3,3,3,11);logo.rect(x,y,3,3,7 if yy<3 else 6)
 logo.rect(29,36,195,1,4);logo.rect(38,39,178,1,9)
 for ty in range(6):
  for tx in range(32):
   t=160+ty*32+tx
   for yy in range(8):
    for xx in range(8):a.dot((t%16)*8+xx,(t//16)*8+yy,logo.p[(ty*8+yy)*256+tx*8+xx])
 a.save('tiles')
 # Sprite atlas: each 16x16 sprite occupies 2x2 tiles in a 128px-wide atlas.
 s=Art(128,128)
 def person(x,y,shirt,hair):
  s.rect(x+4,y+1,8,3,hair);s.rect(x+3,y+3,10,5,10);s.rect(x+3,y+3,3,3,hair);s.dot(x+10,y+4,0)
  s.rect(x+5,y+8,7,5,shirt);s.rect(x+3,y+9,2,4,10);s.rect(x+12,y+9,2,4,10)
  s.rect(x+5,y+13,3,3,11);s.rect(x+10,y+13,3,3,11)
 for i in range(5):person(i*16,0,[7,6,5,15,8][i],[9,9,9,2,3][i])
 # bug: angular circuit beetle, two animation frames.
 for x in (0,16):
  y=32;s.rect(x+4,y+4,8,9,14);s.rect(x+5,y+3,6,2,5);s.rect(x+7,y+4,2,9,12)
  for v in (5,9,12):s.rect(x+1,y+v,3,1,5);s.rect(x+12,y+v+(1 if x else 0),3,1,5)
  s.rect(x+5,y+5,2,2,13);s.rect(x+10,y+5,2,2,13);s.dot(x+5,y+6,0);s.dot(x+10,y+6,0)
 # coffee, chip, spark.
 s.rect(35,35,9,9,3);s.rect(44,36,3,6,4);s.rect(45,37,1,4,0);s.rect(36,36,7,2,9);s.rect(36,45,8,1,4)
 for x in (37,40):s.rect(x,32,1,2,2)
 s.rect(51,35,10,10,7);s.rect(53,37,6,6,11)
 for i in range(4):s.rect(50+i*3,33,1,14,6);s.rect(49,35+i*3,14,1,6)
 # twin tyrants, original tall crowned CRT beings, 32x32 apiece.
 for x,color in [(0,5),(32,6)]:
  y=64;s.rect(x+7,y+4,18,21,11);s.rect(x+5,y+9,22,14,color);s.rect(x+8,y+10,16,10,1)
  s.rect(x+9,y+12,6,2,13);s.rect(x+18,y+12,5,2,13);s.dot(x+12,y+13,14);s.dot(x+19,y+13,14)
  s.rect(x+11,y+17,10,1,color);s.rect(x+7,y+5,18,3,4)
  for k in (8,15,22):s.rect(x+k,y+1,2,5,4)
  s.rect(x+2,y+16,4,10,color);s.rect(x+26,y+16,4,10,color)
  s.rect(x+8,y+25,5,5,2);s.rect(x+20,y+25,5,5,2)
 # Off-By-One: an original 32px circuit beetle boss with an extra antenna.
 x=64;y=64
 s.rect(x+8,y+8,16,18,14);s.rect(x+10,y+6,12,3,5);s.rect(x+15,y+7,2,19,12)
 for dx in (4,14,24):s.rect(x+dx,y+2,2,8,4)
 for dy in (12,18,24):s.rect(x+2,y+dy,6,2,5);s.rect(x+24,y+dy,6,2,5)
 s.rect(x+9,y+11,6,4,13);s.rect(x+18,y+11,6,4,13);s.rect(x+12,y+12,2,3,0);s.rect(x+19,y+12,2,3,0)
 s.rect(x+11,y+21,10,2,4)
 # Hourglass (RATE-LIMITER) and Auditor eye, 32x32 each, atlas rows y=96.
 x=0;y=96
 s.rect(x+4,y+1,24,3,4);s.rect(x+4,y+28,24,3,4);s.rect(x+6,y+4,20,2,13);s.rect(x+6,y+26,20,2,13)
 for k in range(10):
  w=18-k*2;s.rect(x+16-w//2,y+6+k,w,1,13);s.rect(x+16-w//2,y+25-k,w,1,13)
  if k>=1:s.rect(x+16-(w-4)//2,y+6+k,w-4,1,4)
 s.rect(x+15,y+15,2,2,13);s.rect(x+11,y+22,10,3,4);s.rect(x+4,y+4,2,24,9);s.rect(x+26,y+4,2,24,9)
 x=32;y=96
 s.rect(x+2,y+10,28,12,13);s.rect(x+6,y+7,20,3,13);s.rect(x+6,y+22,20,3,13)
 s.rect(x+10,y+9,12,14,14);s.rect(x+13,y+12,6,8,0);s.rect(x+14,y+13,2,2,13)
 s.rect(x+2,y+9,28,2,11);s.rect(x+2,y+21,28,2,11)
 for k in (5,10,16,21,26):s.rect(x+k,y+3,2,5,11);s.rect(x+k,y+25,2,5,11)
 # THE INCIDENT: small shadow blob 16x16 (64,96), big one 32x32 (96,96), sun 16x16 (64,112).
 x=64;y=96
 s.rect(x+3,y+4,10,9,11);s.rect(x+2,y+7,12,5,11);s.rect(x+5,y+6,2,2,14);s.rect(x+9,y+6,2,2,14);s.rect(x+4,y+13,2,2,11);s.rect(x+9,y+13,3,2,11)
 x=96;y=96
 s.rect(x+6,y+6,20,20,11);s.rect(x+3,y+12,26,12,11);s.rect(x+10,y+12,4,4,14);s.rect(x+18,y+12,4,4,14);s.rect(x+11,y+13,2,2,0);s.rect(x+19,y+13,2,2,0)
 s.rect(x+10,y+20,12,2,12)
 for k in (4,11,18,25):s.rect(x+k,y+26,3,5,11)
 s.rect(x+2,y+2,3,6,11);s.rect(x+27,y+4,3,6,11)
 x=64;y=112
 s.rect(x+4,y+4,8,8,4);s.rect(x+3,y+5,10,6,4);s.rect(x+5,y+6,2,2,13)
 for k in (0,7,14):s.rect(x+k,y+7,2,2,4)
 for k in (0,7,14):s.rect(x+7,y+k,2,2,4)
 s.save('sprites')
 print('Original PNG atlases converted to 4bpp tiles.')
if __name__=='__main__':main()
