#!/usr/bin/env python3
################################################################################
from obt import path, command
from obt.pathtools import ensureDirectoryExists
from obt.wget import batch_wget
from yarl import URL
################################################################################
dest_path = path.stage()/"share"/"singularity"
base_manuals = URL("http://www.synthmanuals.com/manuals")
################################################################################
ensureDirectoryExists(dest_path)
################################################################################
# tx81Z data
################################################################################
ensureDirectoryExists(dest_path/"tx81z")
base_81zManuals = base_manuals/"yamaha"/"tx81z"
manurl = base_81zManuals/"owners_manual"/"yamaha_tx81z_owners_manual_rvgm.pdf"
base_81zurl = URL("http://nuxmicromedia.com/4ophq/files-4op-patches/")
ym2151manurl = URL("http://map.grauw.nl/resources/sound/yamaha_ym2151_synthesis.pdf")
batch_wget({
 manurl:(dest_path/"tx81z"/"tx81z_manual2.0.pdf","77290fc492848ddd7c574f23bad4686c"),
 ym2151manurl:(dest_path/"tx81z"/"ym2151_manual.pdf","623a1279464a60ff9a01016af9f242e3"),
 base_81zurl/"TX81Z_Presets.zip": (dest_path/"tx81z"/"TX81Z_Presets.zip","d9f3a7c9eb721e7c06490905b4769829"),
})
(dest_path/"tx81z").chdir()
command.system(["unzip","-j","-o","TX81Z_Presets.zip"])
################################################################################
base_casioCZ = URL("http://cd.textfiles.com/10000soundssongs/SYNTHDAT/CASIO")
base_casioManuals = base_manuals/"casio"
ensureDirectoryExists(dest_path/"casioCZ")
batch_wget({

 base_casioCZ/"FACTRYA.BNK": (dest_path/"casioCZ"/"factoryA.bnk","25e5a50fcaea8ce351a59d67d91c6284"),
 base_casioCZ/"FACTRYB.BNK": (dest_path/"casioCZ"/"factoryB.bnk","cb7d91d5fed5b283c0eb12a1b59d83d9"),
 base_casioCZ/"CZ1_1.BNK": (dest_path/"casioCZ"/"cz1_1.bnk","00ec49470a8fd608dc4c99ade63da9e1"),
 base_casioCZ/"CZ1_2.BNK": (dest_path/"casioCZ"/"cz1_2.bnk","63efea6b5a72a6ddec9a169f55af7176"),
 base_casioCZ/"CZ1_3.BNK": (dest_path/"casioCZ"/"cz1_3.bnk","28b4b9e3299723290d3e2dd0b573e760"),
 base_casioCZ/"CZ1_4.BNK": (dest_path/"casioCZ"/"cz1_4.bnk","08a090716825288b26ee163fd2be85a2"),

 # bad format or duplicates
 #base_casioCZ/"JXBANK.BNK": (dest_path/"casioCZ"/"jxbank.bnk","78493d68c279ee201da82c3b478ed013"),
 #base_casioCZ/"DXBANK.BNK": (dest_path/"casioCZ"/"dxbank.bnk","1dc0521a0b62ab8339f77fdfcef6a997"),
 #base_casioCZ/"FX.BNK": (dest_path/"casioCZ"/"fx.bnk","04f6b8837d66515aebaba20b6f60a9f4"),
 #base_casioCZ/"SYNTHS.BNK": (dest_path/"casioCZ"/"synths.bnk","f278ec63496eed601d4c22fcdeeac655"),
 #base_casioCZ/"CZ1STUFF.BNK": (dest_path/"casioCZ"/"cz1stuff.bnk","00ec49470a8fd608dc4c99ade63da9e1"),


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
if False: 
  batch_wget({
   base_soundfont/"Fender2.sf2": (dest_path/"soundfont"/"fender2.sf2","4b59fa284e460c16ce1077ce6ade8f0e"),
   base_soundfont/"Psr-27.sf2": (dest_path/"soundfont"/"psr27.sf2","c29c727802841d73fac517d67eb00a8e"),
   base_soundfont/"aahhs.sf2": (dest_path/"soundfont"/"aahhs.sf2","d8a52312c5d331c0ee929f6dd362a7e7"),
   base_soundfont/"org_harp.sf2": (dest_path/"soundfont"/"org_harp.sf2","e166472f6299a8e2fc539b3309b2fffc"),
   base_soundfont/"Cadenza.syx": (dest_path/"soundfont"/"cadenza.syx","d41d8cd98f00b204e9800998ecf8427e"),
  })
################################################################################
base_k2000man = base_manuals/"kurzweil"/"k2000"
base_kurzweil = URL("https://media.sweetwater.com/k2000/ftp-files/files")
dest_kurzweil = dest_path/"kurzweil"
ensureDirectoryExists(dest_path/"kurzweil")

batch_wget({
 #ase_kurzweil/"idstperc.krz": (dest_kurzweil/"idstperc.krz","5b35772d5a0fb984c95a52f3af1b5ae0"),
 base_kurzweil/"timpgcff.krz": (dest_kurzweil/"timpgcff.krz","69293fc0fe38fbca9552aaeae1cd278a"),
 base_kurzweil/"gong.krz": (dest_kurzweil/"gong.krz","c5ed60ffa3fbc4c2194b8ebaf7875887"),
 base_kurzweil/"african.krz": (dest_kurzweil/"african.krz","74c4d3c0e94cedac8366510467724085"),
 base_kurzweil/"indiperc.krz": (dest_kurzweil/"indiperc.krz","68aeab00e24b1db0321e0c0c8ec6c7d2"),
 base_kurzweil/"flute.krz": (dest_kurzweil/"flute.krz","775ef65f1db4262dffe259da44c07206"),
 base_kurzweil/"sledge.krz": (dest_kurzweil/"sledge.krz","afd65a4e12414a337fd49a83959ca5b1"),
 base_kurzweil/"guitloop.krz": (dest_kurzweil/"guitloop.krz","39e6c70bf33f57c5ba5b7c92d3c16b44"),
 base_kurzweil/"dreamgtr.krz": (dest_kurzweil/"dreamgtr.krz","93a8d4c05abe5cac7ab61cbd0e67ca2e"),
 base_kurzweil/"classgtr.krz": (dest_kurzweil/"classgtr.krz","18b26c64eff23efca0bcc8b135bf3842"),
 base_kurzweil/"acguit.krz": (dest_kurzweil/"acguit.krz","1ae29993d77a275e54a9931e81b540af"),
 base_kurzweil/"rockgtr1.krz": (dest_kurzweil/"rockgtr1.krz","433c5036194c51d2c20e38da4714f3cd"),
 base_kurzweil/"harpsi.krz": (dest_kurzweil/"harpsi.krz","ef390351f082d1af889b8321e3da61b4"),
 base_kurzweil/"pipeorgn.krz": (dest_kurzweil/"pipeorgn.krz","d35822e1e5471230bd6972da428aa112"),
 base_kurzweil/"b3.krz": (dest_kurzweil/"b3.krz","d7fa0d0c506d06a2d0d08f9c584f13a6"),
 base_kurzweil/"piano.krz": (dest_kurzweil/"piano.krz","814e041d1dbc6e67a7b9dda4efb1544c"),
 base_kurzweil/"steinway.krz": (dest_kurzweil/"steinway.krz","cfbb7b719ae5c1adc185fbe89450d9fd"),
 base_kurzweil/"rapkit.krz": (dest_kurzweil/"rapkit.krz","dc0e6d1bd5341d2280820cd5d9793a7b"),
 #base_kurzweil/"koreaprc.krz": (dest_kurzweil/"koreaprc.krz","53e2d4a13472ad556bd11af4efbd0039"),
 #base_kurzweil/"r100drum.krz": (dest_kurzweil/"r100drum.krz","b99f5b63c4657e7b66705d64b6f362cf"),
 base_kurzweil/"choicdrm.krz": (dest_kurzweil/"choicdrm.krz","f3d4439d361fbf3d5b502630d989be85"),
 base_kurzweil/"dx7works.krz": (dest_kurzweil/"dx7works.krz","b8eeafedd5e57353a0176ae3feab3767"),
 base_kurzweil/"boychoir.krz": (dest_kurzweil/"boychoir.krz","1cff679da152b9e9a68e19c3bc6d4d0a"),
 base_kurzweil/"monopoly.krz": (dest_kurzweil/"monopoly.krz","d3063a159e5ae27619810b82cc3c0531"),
 base_kurzweil/"lostring.krz": (dest_kurzweil/"lostring.krz","e1d520463213c82bd732e0f108c8daec"),
 base_kurzweil/"histring.krz": (dest_kurzweil/"histring.krz","a88ca0069a4762006bf3f663902728aa"),
 base_kurzweil/"violin.krz": (dest_kurzweil/"violin.krz","6c874ccff0733de4a0eea8ce0c8bb0ee"),
 base_kurzweil/"bothfrhn.krz": (dest_kurzweil/"bothfrhn.krz","2e3b3599e82f374a40e5e2390cec7596"),
 base_kurzweil/"trumpets.krz": (dest_kurzweil/"trumpets.krz","6891d2e5c77f36c99a1912dbc6b5681d"),
 base_kurzweil/"quartet.krz": (dest_kurzweil/"quartet.krz","7aa3c3ec11ca70d012795b110658cfd0"),
 base_kurzweil/"rain_1.krz": (dest_kurzweil/"rain_1.krz","031010836e459a0330dec77cf4992bdd"),
 base_kurzweil/"moogbass.krz": (dest_kurzweil/"moogbass.krz","f45a665bf2591bf5edc4f211c73fdd88"),
 #base_kurzweil/"seavoice.krz": (dest_kurzweil/"seavoice.krz","0367caf0df309101c4abb82e5f8d53de"),
 #base_kurzweil/"storms.krz": (dest_kurzweil/"storms.krz","c0400b99f3cf049751930ce793353f99"),
 #base_kurzweil/"bigbells.krz": (dest_kurzweil/"bigbells.krz","31a1c64fe54d5a163e6af13ec80376e3"),
 #base_kurzweil/"akaifunk.krz": (dest_kurzweil/"akaifunk.krz","1a2e0c78ec392e19bde0bb8ea942829d"),
 base_kurzweil/"drpad.krz": (dest_kurzweil/"drpad.krz","b92d808aecd754ca031447ba8e08fe16"),
 base_kurzweil/"alesisdr.krz": (dest_kurzweil/"alesisdr.krz","72fdb9e33845d82a14712ff6557d4ac3"),
 base_kurzweil/"emusp12.krz": (dest_kurzweil/"emusp12.krz","4fec33aeb21fac6e0c1eba4404954c30"),
 base_kurzweil/"m1drums.krz": (dest_kurzweil/"m1drums.krz","f41adb23cbe5a5d08675b09cd32d4c2a"),
 #base_kurzweil/"m1univrs.krz": (dest_kurzweil/"m1univrs.krz","d4e721bc511d56395f4bc10849209b99"),
 base_kurzweil/"cp70.krz": (dest_kurzweil/"cp70.krz","ddd309fa9a11aef6b640372ea87df1ba"),
 base_kurzweil/"epsstrng.krz": (dest_kurzweil/"epsstrng.krz","145d5fb10e347f4ba92f5291a8850c69"),
 base_kurzweil/"monksvox.kr1.krz": (dest_kurzweil/"monksvox.kr1.krz","9fd7144603a2aa3eb98c60a29dcb4790"),
 base_kurzweil/"monksvox.kr2.krz": (dest_kurzweil/"monksvox.kr2.krz","e9a0befc2b63dfd6edf5cd5eea889010"),
 base_k2000man/"k2vx_reference_guide"/"k2vx.pdf": (dest_kurzweil/"k2vx.pdf","bd129f4b014c7a596420fe2ce9efb73c"),
 base_k2000man/"series_musicians_guide"/"k2000_series_musicians_guide.pdf": (dest_kurzweil/"k2000_series_musicians_guide.pdf","5814572ea8f5e8492e128dac08feea66"),
})

ensureDirectoryExists(dest_path/"midifiles")

batch_wget({
  "https://bitmidi.com/uploads/22424.mid": (dest_path/"midifiles"/"castle1.mid","96320847155ffc38e72a5b658259c465"),
  "https://bitmidi.com/uploads/107580.mid": (dest_path/"midifiles"/"castle2.mid","706e48788695b6aca723e2cdad03f097"),
  "https://bitmidi.com/uploads/107583.mid": (dest_path/"midifiles"/"castle3.mid","b370bcb8c9fdced91f174d9376154d5a"),
  "https://bitmidi.com/uploads/16752.mid": (dest_path/"midifiles"/"moonlight.mid","3cb2f37a8f74f93a60d31c1b818ba43f"),
})

ensureDirectoryExists(dest_path/"wavs")

wav_base = URL("https://github.com/sgossner/VSCO-2-CE/raw/master/VSCO%201%20Percussion/drums")
wav_base2 = URL("https://github.com/sgossner/VSCO-2-CE/raw/master/Strings/Violin%20Section")

batch_wget({
  wav_base/"bass/bdrum2_pp_1.wav": (dest_path/"wavs"/"bdrum2_pp_1.wav","4fb66f1bcb4e61c4afc5e30856dc0b0d"),
  wav_base/"bass/bdrum2_pp_2.wav": (dest_path/"wavs"/"bdrum2_pp_2.wav","16d5ea349fa3e417abac7000fa213ed5"),
  wav_base/"bass/bdrum_f_1.wav": (dest_path/"wavs"/"bdrum_f_1.wav","9b9d75dc4e1afec44d1c470f9d169bdb"),
  wav_base/"bass/bdrum_f_2.wav": (dest_path/"wavs"/"bdrum_f_2.wav","3261005b0abd0821eeb3de8e046563ab"),
  wav_base/"snare/OldSnare/snare_f3.wav": (dest_path/"wavs"/"snare_f3.wav","9259193b9fa91197f09550b577fdf92c"),
  wav_base2/"Trem/VlnEns_Trem_A2_v1.wav": (dest_path/"wavs"/"VlnEns_Trem_A2_v1.wav","b40592fb499763f86eae4c39bdaad033")
})

batch_wget({
  URL("http://www.tweakoz.com/resources/audio/Feb142023.mp3"): (dest_path/"wavs"/"feb142023.mp3","d6ade30fdc8f9c80a7c846c983b4877a")})