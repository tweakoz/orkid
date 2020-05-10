#!/usr/bin/env python3
################################################################################
from ork import path
from ork.pathtools import ensureDirectoryExists
from ork.wget import batch_wget
from yarl import URL
################################################################################
dest_path = path.stage()/"share"/"singularity"
################################################################################
ensureDirectoryExists(dest_path)
################################################################################
# tx81Z data
################################################################################
ensureDirectoryExists(dest_path/"tx81z")
base_81zurl = str("http://www.sysexdb.com/binaryview.aspx?IDSysexMessage=")
batch_wget({
 base_81zurl+"1758": (dest_path/"tx81z"/"factorypatchA.syx","a14c5af8fb22c4c4ff2fb3123dd2aa6b"),
 base_81zurl+"1759": (dest_path/"tx81z"/"factorypatchB.syx","05827dd45f6010ee5ae62790b453dcb2"),
 base_81zurl+"1760": (dest_path/"tx81z"/"factorypatchC.syx","58543b3f0fa726e0ff746b59932b152a"),
 base_81zurl+"1761": (dest_path/"tx81z"/"factorypatchD.syx","eddfa391958b64695de51ec5c86f6d5f"),
})
################################################################################
base_casioCZ = URL("http://cd.textfiles.com/10000soundssongs/SYNTHDAT/CASIO")
base_casioManuals = URL("http://www.synthmanuals.com/manuals/casio")
ensureDirectoryExists(dest_path/"casioCZ")
batch_wget({
 base_casioCZ/"FACTRYA.BNK": (dest_path/"casioCZ"/"factoryA.bnk","25e5a50fcaea8ce351a59d67d91c6284"),
 base_casioCZ/"FACTRYB.BNK": (dest_path/"casioCZ"/"factoryB.bnk","cb7d91d5fed5b283c0eb12a1b59d83d9"),
 base_casioCZ/"CZ1_1.BNK": (dest_path/"casioCZ"/"cz1_1.bnk","00ec49470a8fd608dc4c99ade63da9e1"),
 base_casioCZ/"CZ1_2.BNK": (dest_path/"casioCZ"/"cz1_2.bnk","63efea6b5a72a6ddec9a169f55af7176"),
 base_casioCZ/"CZ1_3.BNK": (dest_path/"casioCZ"/"cz1_3.bnk","28b4b9e3299723290d3e2dd0b573e760"),
 base_casioCZ/"CZ1_4.BNK": (dest_path/"casioCZ"/"cz1_4.bnk","08a090716825288b26ee163fd2be85a2"),
 base_casioManuals/"cz-1"/"owners_manual"/"casio_cz-1_owners_manual.pdf":
 (dest_path/"casioCZ"/"cz1manual.pdf","8e3767ac690af435dd1d9371fe494d1d"),
 base_casioManuals/"cz-5000"/"owners_manual"/"cz5000ownersmanual.pdf":
 (dest_path/"casioCZ"/"cz500manual.pdf","f07cb2db66b6c91a0677602293a55f14"),
 "http://thesnowfields.com/manuals/An%20Insider's%20Guide%20to%20Casio%20CZ%20Synthesizers.pdf":
(dest_path/"casioCZ"/"insidersguide.pdf","3df8a020eeb5dc019a3dfd58244fe2ec")

})
################################################################################
base_soundfont = URL("ftp://ftp.lysator.liu.se/pub/awe32/soundfonts")
ensureDirectoryExists(dest_path/"soundfont")
batch_wget({
 base_soundfont/"Fender2.sf2": (dest_path/"soundfont"/"fender2.sf2","4b59fa284e460c16ce1077ce6ade8f0e"),
 base_soundfont/"Psr-27.sf2": (dest_path/"soundfont"/"psr27.sf2","c29c727802841d73fac517d67eb00a8e"),
 base_soundfont/"aahhs.sf2": (dest_path/"soundfont"/"aahhs.sf2","d8a52312c5d331c0ee929f6dd362a7e7"),
 base_soundfont/"org_harp.sf2": (dest_path/"soundfont"/"org_harp.sf2","e166472f6299a8e2fc539b3309b2fffc"),
 base_soundfont/"Cadenza.syx": (dest_path/"soundfont"/"cadenza.syx","d41d8cd98f00b204e9800998ecf8427e"),
})
################################################################################
base_kurzweil = URL("https://media.sweetwater.com/k2000/ftp-files/files")
ensureDirectoryExists(dest_path/"kurzweil")
batch_wget({
 base_kurzweil/"bigbells.krz": (dest_path/"kurzweil"/"bigbells.krz","31a1c64fe54d5a163e6af13ec80376e3"),
 base_kurzweil/"akaifunk.krz": (dest_path/"kurzweil"/"akaifunk.krz","1a2e0c78ec392e19bde0bb8ea942829d"),
 base_kurzweil/"drpad.krz": (dest_path/"kurzweil"/"drpad.krz","b92d808aecd754ca031447ba8e08fe16"),
 base_kurzweil/"alesisdr.krz": (dest_path/"kurzweil"/"alesisdr.krz","72fdb9e33845d82a14712ff6557d4ac3"),
 base_kurzweil/"emusp12.krz": (dest_path/"kurzweil"/"emusp12.krz","4fec33aeb21fac6e0c1eba4404954c30"),
 base_kurzweil/"m1drums.krz": (dest_path/"kurzweil"/"m1drums.krz","f41adb23cbe5a5d08675b09cd32d4c2a"),
 base_kurzweil/"m1univrs.krz": (dest_path/"kurzweil"/"m1univrs.krz","d4e721bc511d56395f4bc10849209b99"),
 base_kurzweil/"cp70.krz": (dest_path/"kurzweil"/"cp70.krz","ddd309fa9a11aef6b640372ea87df1ba"),
 base_kurzweil/"epsstrng.krz": (dest_path/"kurzweil"/"epsstrng.krz","145d5fb10e347f4ba92f5291a8850c69"),
 base_kurzweil/"monksvox.kr1.krz": (dest_path/"kurzweil"/"monksvox.kr1.krz","9fd7144603a2aa3eb98c60a29dcb4790"),
 base_kurzweil/"monksvox.kr2.krz": (dest_path/"kurzweil"/"monksvox.kr2.krz","e9a0befc2b63dfd6edf5cd5eea889010"),
})
