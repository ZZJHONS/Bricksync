/* -----------------------------------------------------------------------------
 *
 * Copyright (c) 2014-2019 Alexis Naveros.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * -----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"


/*
gcc bnbuild.c bn128.c bn192.c bn256.c bn512.c bn1024.c cc.c -O2 -fomit-frame-pointer -funroll-loops -s -o bnbuild -lm -Wall

gcc bnbuild.c bn128.c bn192.c bn256.c bn512.c bn1024.c cc.c -O2 -fomit-frame-pointer -funroll-loops -g -o bnbuild -lm -Wall
*/


////


/*
#define BN_XP_BITS (128)
*/
/*
#define BN_XP_BITS (192)
*/
/*
#define BN_XP_BITS (256)
*/
/*
#define BN_XP_BITS (512)
*/
#define BN_XP_BITS (1024)


#define BN_XP_SUPPORT_128 (1)
#define BN_XP_SUPPORT_192 (1)
#define BN_XP_SUPPORT_256 (1)
#define BN_XP_SUPPORT_512 (1)
#define BN_XP_SUPPORT_1024 (1)
#include "bn.h"


////


#define FE_FIXED_POINT_OFFSET (BN_XP_BITS-8)


#define FE_INT16_FIXED_SHIFT (BN_XP_BITS-16)


////




#define BN_LOG2_SHIFT (BN_XP_BITS)
static char *bnLog2String = "0.69314718055994530941723212145817656807550013436025525412068000949339362196969471560586332699641868754200148102057068573368552023575813055703267075163507596193072757082837143519030703862389167347112335011536449795523912047517268157493206515552473413952588295045300709532636664265410423915781495204374043038550080194417064167151864471283996817178454695702627163106454615025720740248163777338963855069526066834113727387372292895649354702576265209885969320196505855476470330679365443254763274495125040606943814710468994650622016772042452452961268794654619316517468139267250410380254625965686914419287160829380317271436778265487756648508567407764845146443994046142260319309673540257444607030809608504748663852313818167675143866747664789088143714198549423151997354880375165861275352916610007105355824987941472950929311389715599820565439287170007218085761025236889213244971389320378439353088774825970171559107088236836275898425891853530243634214367061189236789192372314672321720534016492568727477823445353476481149418642386776774406069562657379600867076257199184734022651462837904883062033061144630073719489002743643965002580936519443041191150608094879306786515887090060520346842973619384128965255653968602219412292420757432175748909770675268711581705113700915894266547859596489065305846025866838294002283300538207400567705304678700184162404418833232798386349001563121889560650553151272199398332030751408426091479001265168243443893572472788205486271552741877243002489794540196187233980860831664811490930667519339312890431641370681397776498176974868903887789991296503619270710889264105230924783917373501229842420499568935992206602204654941510613918788574424557751020683703086661948089641218680779020818158858000168811597305618667619918739520076671921459223672060253959543654165531129517598994005600036651356756905124592682574394648316833262490180382424082423145230614096380570070255138770268178516306902551370323405380214501901537402950994226299577964742713";

#define BN_LOG1P0625_SHIFT (BN_XP_BITS)
static char *bnLog1p0625String = "0.06062462181643484258060613204042026328620247514472377081451769990871808792215241848864156005182819735034233796490431435665551189891421613616907155920171742353700292475860473831524352722959996641211510644773882214038866739447346579474360877115587676036541264856377193434056335997322031337201382158431227260623923920753299973635981610447290560160748245715335297092661820178500675600344551692297766325578092455814575339390774394478288327005416467645723555082150200399444895566101544850846514893393492437331536253168161606501654529257980836386493158245561314188631354634315802997295669267006695008899818464570499565479899193460004234791922107890886102792261241765441597857504735723303889511389087998669112691394834642667954028659066210866563392493191174120207541646441595381376659293877325583889057099027020498629738398312134534068460992696774269144131263638850318400924157348622841844908029672613984727124386762386656145491905758218206290787562081494814624626163160437080637363643006767970097393828252808065354408545718570466079033422966287956646847848361583566038076167054822243692230111488588045235404311774203415688512881002400528153945782207662148309618833472791641780618383709496232327515136348256033735680845295206318296077660960992070444367044847525985639967370279490042802044296223424065027452462865098439140121411954533966103175262027779657127605250338375842413046845237292522633895806768724268824498987265096772840099443132958213089386879818778200188999113707643713272378212387182323901625204313832200677809679824382100842865266882935849186876072091996099349498223365021047113288580247273303426224733091814537041850664951526308158009620463242014971427964847911301864123432867538090473664113698216440908917319182842760779963561540087556045051356672471034339201310938020301289206529343643434946889360956648577015373194838647712998328064253787039901689775665493535254721656909388228236044799882052561429779077625780229601545276156524217865123149";

#define BN_MUL1P0625_SHIFT (BN_XP_BITS-1)

#define BN_DIV1P0625_SHIFT (BN_XP_BITS)

#define BN_EXP1_SHIFT (BN_XP_BITS-2)
static char *bnExp1String = "2.71828182845904523536028747135266249775724709369995957496696762772407663035354759457138217852516642742746639193200305992181741359662904357290033429526059563073813232862794349076323382988075319525101901157383418793070215408914993488416750924476146066808226480016847741185374234544243710753907774499206955170276183860626133138458300075204493382656029760673711320070932870912744374704723069697720931014169283681902551510865746377211125238978442505695369677078544996996794686445490598793163688923009879312773617821542499922957635148220826989519366803318252886939849646510582093923982948879332036250944311730123819706841614039701983767932068328237646480429531180232878250981945581530175671736133206981125099618188159304169035159888851934580727386673858942287922849989208680582574927961048419844436346324496848756023362482704197862320900216099023530436994184914631409343173814364054625315209618369088870701676839642437814059271456354906130310720851038375051011574770417189861068739696552126715468895703503540212340784981933432106817012100562788023519303322474501585390473041995777709350366041699732972508868769664035557071622684471625607988265178713419512466520103059212366771943252786753985589448969709640975459185695638023637016211204774272283648961342251644507818244235294863637214174023889344124796357437026375529444833799801612549227850925778256209262264832627793338656648162772516401910590049164499828931505660472580277863186415519565324425869829469593080191529872117255634754639644791014590409058629849679128740687050489585867174798546677575732056812884592054133405392200011378630094556068816674001698420558040336379537645203040243225661352783695117788386387443966253224985065499588623428189970773327617178392803494650143455889707194258639877275471096295374152111513683506275260232648472870392076431005958411661205452970302364725492966693811513732275364509888903136020572481765851180630364428123149655070475102544650117272115551948668508003685322818";

#define BN_EXP1INV_SHIFT (BN_XP_BITS)
static char *bnExp1InvString = "0.36787944117144232159552377016146086744581113103176783450783680169746149574489980335714727434591964374662732527684399520824697579279012900862665358949409878309219436737733811504863899112514561634498771997868447595793974730254989249545323936620796481051464752061229422308916492656660036507457728370553285373838810680478761195682989345449735073931859921661743300356993720820710227751802158499423378169071566767176233660823037612291562375720947000704050973342567757625252803037688616515709365379954274063707178784454194674909313069805601637021113897742282140173802328324652872913890046609866595124440976998514591642878037202025102245787321110595377768074371122062400051679652809754447802864860068385642004336846624843493869182620625189948219709924234252075104920934452851244860224513809864174210612195363683100782092248046530798065628541547860617931557059871702159996991882282653979278037471274386351562967145119439867026824526797168143897721413595796905425291035488597310782332694141185792356959493769860126575880312799846794846735134680226530244627705698244386387297002987588809534112675423789026164331040918607012257175816660345100790985888080842868407400134038381320004056775831406199265584918451278068708378191982028128450226420817302243546010954123388715759825373768259374426181082619747186504634174528732482726376615837639334212786293584446186772265003530351310971402261145920211374320967130559545051472046499276082391023347323952014953044680019080891010374128078475393591606116410798622192916656241572869710196363694437583635008455264959360732718331529311490245946159045476975931013890843997519331333484334772006962859487435285562973916515075740987283819153855057361331802861240591869093738759095604427125118632205309731798364031604200974914428425552273699179720997575637824556008073252479767214584985171037214044066748731577721203994697467609716825710177427020919766253195826039309222941027267245447113444753506162288776919476168669507108871169";

#define BN_EXP0P125_SHIFT (BN_XP_BITS-1)
static char *bnExp0p125String = "1.13314845306682631682900722781179387256550313174518162591282003607882357788004838651393999079494172857323152701564730756570482104525847339987855640259169952611627592807003979847293203456303406594694353727210578799691705039784490022263632424121050787414990738497037594394780744214172427088736048030923213268237603036088416828681032544848018954487813996029052341056508249273033195516259611033993052957841241651177817941167302874314336274048309797109721413383205245206496390016093302847955883332965881508587748782903615417837096086356955827981651671325361624035923988088685911411436999721926739385611682074633498945906139325152749573282537321288144737488765166924118101216044621089000437179273964748453386064202577193934595757885426908674072266346254207633319258981138250170237956454431952321281797473748885008772140244648646955003481537392522917885085923621594875295749431910575415082427054943675593105341047861107472886504744084705661532469303480822991821101018921897115866912313846313000365283011985003884913211065784183222152901571455202981898592832583081313905763862969649514471003443032501105136131998855990816396279180935695891303989830382015143815384761052143516138523042353948549179888464532233663779016268736097317796716965107433014560809952155597984034636492614500012591649070868401193200781208389624268722172634292104669452831800012096437862544190277649461438374460729596906354575950921121339008477673369426555009405050574050989315787219292465621240412670327008346218596697074169473740593239156430200614286634698791610466754740250207659112807909829820119043813281242151181891261558645413740122908196364464119051764344622174774042343973794837351170486587163440877267464924725947578476641533030745454495894111467501144916806175684594107841369555104226291750121808027602375691753576967951877275679011228666555015205960963583849032874273347472240974881098080730356120090622929358165549807570515249314138603556062678335824446399064355197373056091";

#define BN_PI_SHIFT (BN_XP_BITS-2)
static char *bnPiString = "3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316527120190914564856692346034861045432664821339360726024914127372458700660631558817488152092096282925409171536436789259036001133053054882046652138414695194151160943305727036575959195309218611738193261179310511854807446237996274956735188575272489122793818301194912983367336244065664308602139494639522473719070217986094370277053921717629317675238467481846766940513200056812714526356082778577134275778960917363717872146844090122495343014654958537105079227968925892354201995611212902196086403441815981362977477130996051870721134999999837297804995105973173281609631859502445945534690830264252230825334468503526193118817101000313783875288658753320838142061717766914730359825349042875546873115956286388235378759375195778185778053217122680661300192787661119590921642019893809525720106548586327886593615338182796823030195203530185296899577362259941389124972177528347913151557485724245415069595082953311686172785588907509838175463746493931925506040092770167113900984882401285836160356370766010471018194295559619894676783744944825537977472684710404753464620804668425906949129331367702898915210475216205696602405803815019351125338243003558764024749647326391419927260426992279678235478163600934172164121992458631503028618297455570674983850549458858692699569092721079750930295532116534498720275596023648066549911988183479775356636980742654252786255181841757467289097777279380008164706001614524919217321721477235014144197356854816136115735255213347574184946843852332390739414333454776241686251898356948556209921922218427255025425688767179049460165346680498862723279178608578438382796797668145410095388378636095068006422512520511739298489608412848862694560424196528502221066118630674427862203919494504712371378696095636437191728";




void bnprint128x64( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 128
  bnXpShrRound( &b, bn, BN_XP_BITS-128 );
  printf( "%s0x%016llx, 0x%016llx%s", prefix, (long long)bnXpExtract64( &b, 0*64 ), (long long)bnXpExtract64( &b, 1*64 ), suffix );
#endif
  return;
}

void bnprint128x32( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 128
  bnXpShrRound( &b, bn, BN_XP_BITS-128 );
  printf( "%s0x%08x, 0x%08x, 0x%08x, 0x%08x%s", prefix, (int)bnXpExtract32( &b, 0*32 ), (int)bnXpExtract32( &b, 1*32 ), (int)bnXpExtract32( &b, 2*32 ), (int)bnXpExtract32( &b, 3*32 ), suffix );
#endif
  return;
}

void bnprint192x64( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 192
  bnXpShrRound( &b, bn, BN_XP_BITS-192 );
  printf( "%s0x%016llx, 0x%016llx, 0x%016llx%s", prefix, (long long)bnXpExtract64( &b, 0*64 ), (long long)bnXpExtract64( &b, 1*64 ), (long long)bnXpExtract64( &b, 2*64 ), suffix );
#endif
  return;
}

void bnprint192x32( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 192
  bnXpShrRound( &b, bn, BN_XP_BITS-192 );
  printf( "%s0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x%s", prefix, (int)bnXpExtract32( &b, 0*32 ), (int)bnXpExtract32( &b, 1*32 ), (int)bnXpExtract32( &b, 2*32 ), (int)bnXpExtract32( &b, 3*32 ), (int)bnXpExtract32( &b, 4*32 ), (int)bnXpExtract32( &b, 5*32 ), suffix );
#endif
  return;
}

void bnprint256x64( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 256
  bnXpShrRound( &b, bn, BN_XP_BITS-256 );
  printf( "%s0x%016llx, 0x%016llx, 0x%016llx, 0x%016llx%s", prefix, (long long)bnXpExtract64( &b, 0*64 ), (long long)bnXpExtract64( &b, 1*64 ), (long long)bnXpExtract64( &b, 2*64 ), (long long)bnXpExtract64( &b, 3*64 ), suffix );
#endif
  return;
}

void bnprint256x32( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 256
  bnXpShrRound( &b, bn, BN_XP_BITS-256 );
  printf( "%s0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x%s", prefix, (int)bnXpExtract32( &b, 0*32 ), (int)bnXpExtract32( &b, 1*32 ), (int)bnXpExtract32( &b, 2*32 ), (int)bnXpExtract32( &b, 3*32 ), (int)bnXpExtract32( &b, 4*32 ), (int)bnXpExtract32( &b, 5*32 ), (int)bnXpExtract32( &b, 6*32 ), (int)bnXpExtract32( &b, 7*32 ), suffix );
#endif
  return;
}

void bnprint512x64( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 512
  bnXpShrRound( &b, bn, BN_XP_BITS-512 );
  printf( "%s", prefix );
  for( i = 0 ; ; )
  {
    printf( "0x%016llx", (long long)bnXpExtract64( &b, i ) );
    i += 64;
    if( i >= 512 )
      break;
    printf( ", " );
  }
  printf( "%s", suffix );
#endif
  return;
}

void bnprint512x32( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 512
  bnXpShrRound( &b, bn, BN_XP_BITS-512 );
  printf( "%s", prefix );
  for( i = 0 ; ; )
  {
    printf( "0x%08x", (int)bnXpExtract32( &b, i ) );
    i += 32;
    if( i >= 512 )
      break;
    printf( ", " );
  }
  printf( "%s", suffix );
#endif
  return;
}

void bnprint1024x64( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 1024
  bnXpShrRound( &b, bn, BN_XP_BITS-1024 );
  printf( "%s", prefix );
  for( i = 0 ; ; )
  {
    printf( "0x%016llx", (long long)bnXpExtract64( &b, i ) );
    i += 64;
    if( i >= 1024 )
      break;
    printf( ", " );
  }
  printf( "%s", suffix );
#endif
  return;
}

void bnprint1024x32( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 1024
  bnXpShrRound( &b, bn, BN_XP_BITS-1024 );
  printf( "%s", prefix );
  for( i = 0 ; ; )
  {
    printf( "0x%08x", (int)bnXpExtract32( &b, i ) );
    i += 32;
    if( i >= 1024 )
      break;
    printf( ", " );
  }
  printf( "%s", suffix );
#endif
  return;
}




void build()
{
  int i;
  bnXp bn, one, tmp, tmp2;
  char printbuffer[BN_XP_BITS/3+2];


  printf( "Build\n{\n" );


  bnXpScan( &tmp, bnLog2String, BN_LOG2_SHIFT );
  printf( "  Log2\n" );
  printf( "  In  : %s\n", bnLog2String );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  bnprint1024x64( &tmp, "   (1024,64) : ", "\n" );
  bnprint1024x32( &tmp, "   (1024,32) : ", "\n" );
  bnXpPrint( &tmp, printbuffer, sizeof(printbuffer), 0, BN_LOG2_SHIFT, BN_LOG2_SHIFT );
  printf( "Value Check : %s\n\n", printbuffer );

  bnXpScan( &tmp, bnLog1p0625String, BN_LOG1P0625_SHIFT );
  printf( "  Log1p0625\n" );
  printf( "  In  : %s\n", bnLog1p0625String );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  bnprint1024x64( &tmp, "   (1024,64) : ", "\n" );
  bnprint1024x32( &tmp, "   (1024,32) : ", "\n" );
  bnXpPrint( &tmp, printbuffer, sizeof(printbuffer), 0, BN_LOG1P0625_SHIFT, BN_LOG1P0625_SHIFT );
  printf( "Value Check : %s\n\n", printbuffer );

  bnXpSet32( &tmp, 17 );
  bnXpSet32( &tmp2, 16 );
  bnXpDivShl( &tmp, &tmp2, 0, BN_MUL1P0625_SHIFT );
  printf( "  Mul1p0625\n" );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  bnprint1024x64( &tmp, "   (1024,64) : ", "\n" );
  bnprint1024x32( &tmp, "   (1024,32) : ", "\n" );
  bnXpPrint( &tmp, printbuffer, sizeof(printbuffer), 0, BN_MUL1P0625_SHIFT, BN_MUL1P0625_SHIFT );
  printf( "Value Check : %s\n\n", printbuffer );

  bnXpSet32( &tmp, 16 );
  bnXpSet32( &tmp2, 17 );
  bnXpDivShl( &tmp, &tmp2, 0, BN_DIV1P0625_SHIFT );
  printf( "  Div1p0625\n" );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  bnprint1024x64( &tmp, "   (1024,64) : ", "\n" );
  bnprint1024x32( &tmp, "   (1024,32) : ", "\n" );
  bnXpPrint( &tmp, printbuffer, sizeof(printbuffer), 0, BN_DIV1P0625_SHIFT, BN_DIV1P0625_SHIFT );
  printf( "Value Check : %s\n\n", printbuffer );



  bnXpScan( &tmp, bnExp1String, BN_EXP1_SHIFT );
  printf( "  Exp1\n" );
  printf( "  In  : %s\n", bnExp1String );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  bnprint1024x64( &tmp, "   (1024,64) : ", "\n" );
  bnprint1024x32( &tmp, "   (1024,32) : ", "\n" );
  bnXpPrint( &tmp, printbuffer, sizeof(printbuffer), 0, BN_EXP1_SHIFT, BN_EXP1_SHIFT );
  printf( "Value Check : %s\n\n", printbuffer );

  bnXpScan( &tmp, bnExp1InvString, BN_EXP1INV_SHIFT );
  printf( "  Exp1inv\n" );
  printf( "  In  : %s\n", bnExp1InvString );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  bnprint1024x64( &tmp, "   (1024,64) : ", "\n" );
  bnprint1024x32( &tmp, "   (1024,32) : ", "\n" );
  bnXpPrint( &tmp, printbuffer, sizeof(printbuffer), 0, BN_EXP1INV_SHIFT, BN_EXP1INV_SHIFT );
  printf( "Value Check : %s\n\n", printbuffer );


  bnXpScan( &tmp, bnExp0p125String, BN_EXP0P125_SHIFT );
  printf( "  Exp0p125\n" );
  printf( "  In  : %s\n", bnExp0p125String );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  bnprint1024x64( &tmp, "   (1024,64) : ", "\n" );
  bnprint1024x32( &tmp, "   (1024,32) : ", "\n" );
  bnXpPrint( &tmp, printbuffer, sizeof(printbuffer), 0, BN_EXP0P125_SHIFT, BN_EXP0P125_SHIFT );
  printf( "Value Check : %s\n\n", printbuffer );



  bnXpScan( &tmp, bnPiString, BN_PI_SHIFT );
  printf( "  Pi\n" );
  printf( "  In  : %s\n", bnPiString );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  bnprint1024x64( &tmp, "   (1024,64) : ", "\n" );
  bnprint1024x32( &tmp, "   (1024,32) : ", "\n" );
  bnXpPrint( &tmp, printbuffer, sizeof(printbuffer), 0, BN_PI_SHIFT, BN_PI_SHIFT );
  printf( "Value Check : %s\n\n", printbuffer );



  bnXpSet32Shl( &one, 1, BN_XP_BITS-1 );
#if BN_XP_BITS >= 128
  printf( "  Div Table (128,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint128x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (128,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint128x32( &bn, "", " } },\n" );
  }
#endif
#if BN_XP_BITS >= 192
  printf( "  Div Table (192,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint192x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (192,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint192x32( &bn, "", " } },\n" );
  }
#endif
#if BN_XP_BITS >= 256
  printf( "  Div Table (256,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint256x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (256,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint256x32( &bn, "", " } },\n" );
  }
#endif
#if BN_XP_BITS >= 512
  printf( "  Div Table (512,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint512x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (512,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint512x32( &bn, "", " } },\n" );
  }
#endif
#if BN_XP_BITS >= 1024
  printf( "  Div Table (1024,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint1024x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (1024,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint1024x32( &bn, "", " } },\n" );
  }
#endif



  printf( "}\n\n" );
  fflush( stdout );


  return;
}



////



void findprimes()
{
  int i, primecount, primealloc;
  bnXp *primelist;
  bnXp target, quotient, rem, maxsearch;
  char buffer[4096];
  uint64_t t0, t1;

  primealloc = 1048576/4;
  primelist = malloc( primealloc * sizeof(bnXp) );

  bnXpSet32( &primelist[0], 2 );
  primecount = 1;
  bnXpSet32( &target, 3 );
  bnXpSqrt( &maxsearch, &target, 0 );

  t0 = mmGetMillisecondsTime();
  for( ; ; )
  {
    for( i = 0 ; ; i++ )
    {
      if( ( i >= primecount ) || ( bnXpCmpGt( &primelist[i], &maxsearch ) ) )
      {
        bnXpSet( &primelist[primecount++], &target );
        if( primecount == primealloc )
          goto done;
        if( !( i & 0xf ) )
          bnXpSqrt( &maxsearch, &target, 0 );
        break;
      }
      bnXpSet( &quotient, &target );
      bnXpDiv( &quotient, &primelist[i], &rem );
      if( bnXpCmpZero( &rem ) )
        break;
    }
    bnXpAdd32( &target, 2 );
    bnXpAdd32( &maxsearch, 2 );
  }

  done:

  t1 = mmGetMillisecondsTime();
  for( i = 0 ; i < primecount ; i++ )
  {
    bnXpPrint( &primelist[i], buffer, 4096, 1, 0, 0 );
    printf( "Prime %d : %s\n", i+1, buffer );
  }
  printf( "Time : %lld msecs\n", (long long)( t1 - t0 ) );
  free( primelist );

  return;
}



////



int main()
{
  int lowbits, i1;
  double f0, f1;
  bnXp h0, h1, h2, h3;
  char buffer[4096];
  char *v0, *v1;





#if 1

  build();
  return 1;

#endif



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "5.934";
  v1 = "2.293";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpDivShl( &h0, &h1, 0, lowbits );
  bnXpPrint( &h0, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.div(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86div(%.3f,%.3f) = %.16f\n", f0, f1, f0/f1 );

  lowbits = BN_XP_BITS / 2;
  v0 = "793035312.459";
  v1 = "0.237";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpDivShl( &h0, &h1, 0, lowbits );
  bnXpPrint( &h0, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.div(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86div(%.3f,%.3f) = %.16f\n", f0, f1, f0/f1 );



  lowbits = 1;
  v0 = "2.5";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSqrt( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sqrt(%.3f) = %s\n", f0, buffer );
  printf( "  x86sqrt(%.3f) = %.16f\n", f0, sqrt(f0) );

  lowbits = 2;
  v0 = "285329435347.0";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSqrt( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sqrt(%.3f) = %s\n", f0, buffer );
  printf( "  x86sqrt(%.3f) = %.16f\n", f0, sqrt(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "43.023";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSqrt( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sqrt(%.3f) = %s\n", f0, buffer );
  printf( "  x86sqrt(%.3f) = %.16f\n", f0, sqrt(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "0.00000000000000000000934";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSqrt( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sqrt(%.3f) = %s\n", f0, buffer );
  printf( "  x86sqrt(%.3f) = %.16f\n", f0, sqrt(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "95.308";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpLog( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.log(%.3f) = %s\n", f0, buffer );
  printf( "  x86log(%.3f) = %.16f\n", f0, log(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "1.6";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpLog( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.log(%.3f) = %s\n", f0, buffer );
  printf( "  x86log(%.3f) = %.16f\n", f0, log(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "1.7";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpExp( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.exp(%.3f) = %s\n", f0, buffer );
  printf( "  x86exp(%.3f) = %.16f\n", f0, exp(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "-3.8";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpExp( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.exp(%.3f) = %s\n", f0, buffer );
  printf( "  x86exp(%.3f) = %.16f\n", f0, exp(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "2.39";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpExp( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.exp(%.3f) = %s\n", f0, buffer );
  printf( "  x86exp(%.3f) = %.16f\n", f0, exp(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "1.38";
  v1 = "1.72";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpPow( &h2, &h0, &h1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.pow(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86pow(%.3f,%.3f) = %.16f\n", f0, f1, pow(f0,f1) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "4.0";
  v1 = "3.0";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpPow( &h2, &h0, &h1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.pow(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86pow(%.3f,%.3f) = %.16f\n", f0, f1, pow(f0,f1) );

  lowbits = 2;
  v0 = "2.0";
  v1 = "-2.0";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpPow( &h2, &h0, &h1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.pow(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86pow(%.3f,%.3f) = %.16f\n", f0, f1, pow(f0,f1) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "0.3";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpCos( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.cos(%.3f) = %s\n", f0, buffer );
  printf( "  x86cos(%.3f) = %.16f\n", f0, cos(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "-6.2";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpCos( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.cos(%.3f) = %s\n", f0, buffer );
  printf( "  x86cos(%.3f) = %.16f\n", f0, cos(f0) );

  lowbits = BN_XP_BITS - 32;
  v0 = "31545.0";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpCos( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.cos(%.3f) = %s\n", f0, buffer );
  printf( "  x86cos(%.3f) = %.16f\n", f0, cos(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "13.6";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSin( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sin(%.3f) = %s\n", f0, buffer );
  printf( "  x86sin(%.3f) = %.16f\n", f0, sin(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "5.7";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSin( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sin(%.3f) = %s\n", f0, buffer );
  printf( "  x86sin(%.3f) = %.16f\n", f0, sin(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "34.7";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpTan( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.tan(%.3f) = %s\n", f0, buffer );
  printf( "  x86tan(%.3f) = %.16f\n", f0, tan(f0) );

  v0 = "4.9";
  f0 = atof( v0 );
  lowbits = FE_FIXED_POINT_OFFSET;
  bnXpScan( &h0, v0, lowbits );
  bnXpTan( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.tan(%.3f) = %s\n", f0, buffer );
  printf( "  x86tan(%.3f) = %.16f\n", f0, tan(f0) );



  lowbits = 0;
  v0 = "3";
  i1 = 43;
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpPowInt( &h2, &h0, i1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 0 );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.pow(%.3f,%d) = %s\n", f0, i1, buffer );
  printf( "  x86pow(%.3f,%d) = %.16f\n", f0, i1, pow(f0,i1) );






/*
srand( time(0) );
for( ; ; )
{
  h0.unit[0] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[1] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[2] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[3] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[4] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[5] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[6] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[7] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[0] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[1] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[2] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[3] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[4] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[5] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[6] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[7] = (long)rand() + ( (long)(rand()+rand()) << 32 );

  bn512Mul( &h2, &h1, &h0 );
  bn512MulShr( &h3, &h1, &h0, 0 );

  bnprint512x64( &h0, "Src 0  :", "\n" );
  bnprint512x64( &h1, "Src 1  :", "\n" );
  bnprint512x64( &h2, "Res A  :", "\n" );
  bnprint512x64( &h3, "Res B  :", "\n" );

  if( bn512CmpNeq( &h2, &h3 ) )
    exit( 0 );

}
*/




  uint64_t t0, t1;
  bnXp k0,k1,k2,k3,k4,k5,k6,kr;

  bnXpSetDouble( &k0, 2.13185, 510 );
  bnXpSetDouble( &k1, -1.9472, 510 );
  bnXpSetDouble( &k2, 0.94387, 510 );
  bnXpSetDouble( &k3, -0.4192, 510 );
  bnXpSetDouble( &k4, 1.84293, 510 );
  bnXpSetDouble( &k5, 0.72641, 510 );

  sleep( 1 );
  t0 = mmGetMillisecondsTime();
  int i;
  for( i = 0 ; i < 40000000 ; i++ )
  {
    bnXpMulSignedShr( &kr, &k0, &k3, 1 );
    bnXpMulSignedShr( &kr, &k1, &k4, 1 );
    bnXpMulSignedShr( &kr, &k2, &k5, 1 );
  }
  t1 = mmGetMillisecondsTime();
  sleep( 1 );

  printf( "Time : %lld msecs for %d muls of %d bits\n", (long long)( t1 - t0 ), 3*40000000, BN_XP_BITS );







/*
int i;
for( i = 0 ; ; i++ )
{
  lowbits = BN_XP_BITS - 32;

  f0 = (double)rand() / (double)RAND_MAX;
  f1 = (double)rand() / (double)RAND_MAX;
  f0 = 8.0 * f0;
  f1 = 8.0 * ( f1 - 0.5 );

  printf( "Low bits : %d\n", lowbits );
  printf( "  x86pow(%.3f,%.3f) = %.16f\n", f0, f1, pow(f0,f1) );

  bnXpSetDouble( &h0, f0, lowbits );
  bnXpSetDouble( &h1, f1, lowbits );
  bnXpPow( &h2, &h0, &h1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "  bn.pow(%.3f,%.3f) = %s\n", f0, f1, buffer );
}
*/







/*
  bnUnit result[BN_INT256_UNIT_COUNT*2];
  bnUnit result2[BN_INT256_UNIT_COUNT*2];
  bnUnit src0[BN_INT256_UNIT_COUNT];
  bnUnit src1[BN_INT256_UNIT_COUNT];

srand( time(0) );
for( ; ; )
{
  int i;
  memset( src0, 0, BN_INT256_UNIT_COUNT*sizeof(bnUnit) );
  memset( src1, 0, BN_INT256_UNIT_COUNT*sizeof(bnUnit) );
  memset( result, 0, 2*BN_INT256_UNIT_COUNT*sizeof(bnUnit) );
  memset( result2, 0, 2*BN_INT256_UNIT_COUNT*sizeof(bnUnit) );

  for( i = 0 ; i < BN_INT256_UNIT_COUNT ; i++ )
  {
#if BN_UNIT_BITS == 64
    src0[i] = (long)rand() + ( (long)(rand()+rand()) << 32 );
    src1[i] = (long)rand() + ( (long)(rand()+rand()) << 32 );
#else
    src0[i] = (long)rand() + rand();
    src1[i] = (long)rand() + rand();
#endif
  }

  bn256MulExtended( result, src0, src1, 0xffffffff );
  bn256AddMulExtended( result2, src0, src1, 0xffffffff, 0 );

#if BN_UNIT_BITS == 64
  printf( "Src 0    : %016llx %016llx %016llx %016llx\n", (long long)src0[0], (long long)src0[1], (long long)src0[2], (long long)src0[3] );
  printf( "Src 1    : %016llx %016llx %016llx %016llx\n", (long long)src1[0], (long long)src1[1], (long long)src1[2], (long long)src1[3] );
  printf( "Result 1 : %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n", (long long)result[0], (long long)result[1], (long long)result[2], (long long)result[3], (long long)result[4], (long long)result[5], (long long)result[6], (long long)result[7] );
  printf( "Result 2 : %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n", (long long)result2[0], (long long)result2[1], (long long)result2[2], (long long)result2[3], (long long)result2[4], (long long)result2[5], (long long)result2[6], (long long)result2[7] );
#else
  printf( "Src 0    : %08x %08x %08x %08x\n", (int)src0[0], (int)src0[1], (int)src0[2], (int)src0[3] );
  printf( "Src 1    : %08x %08x %08x %08x\n", (int)src1[0], (int)src1[1], (int)src1[2], (int)src1[3] );
  printf( "Result 1 : %08x %08x %08x %08x %08x %08x %08x %08x\n", (int)result[0], (int)result[1], (int)result[2], (int)result[3], (int)result[4], (int)result[5], (int)result[6], (int)result[7] );
  printf( "Result 2 : %08x %08x %08x %08x %08x %08x %08x %08x\n", (int)result2[0], (int)result2[1], (int)result2[2], (int)result2[3], (int)result2[4], (int)result2[5], (int)result2[6], (int)result2[7] );
#endif

if( ( result[0] != result2[0] ) || ( result[1] != result2[1] ) || ( result[2] != result2[2] ) || ( result[3] != result2[3] ) || ( result[4] != result2[4] ) || ( result[5] != result2[5] ) || ( result[6] != result2[6] ) || ( result[7] != result2[7] ) )
exit(0);

}
*/






/*
  findprimes();
*/



  return 1;
}










/*
def log2(x, tol=1e-13):

    res = 0.0
 
    # Integer part
    while x<1:
        res -= 1
        x *= 2
    while x>=2:
        res += 1
        x /= 2
 
    # Fractional part
    fp = 1.0
    while fp>=tol:
        fp /= 2
        x *= x
        if x >= 2:
            x /= 2
            res += fp

     return res
*/


