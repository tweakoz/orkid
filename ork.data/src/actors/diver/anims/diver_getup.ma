//Maya ASCII 2008 scene
//Name: diver_getup.ma
//Last modified: Tue, Aug 26, 2008 02:50:01 PM
//Codeset: 1252
file -rdi 1 -ns "D_" -rfn "D_RN" "V:/projects/w8/data/src//actors/Diver/ref/diver.ma";
file -r -ns "D_" -dr 1 -rfn "D_RN" "V:/projects/w8/data/src//actors/Diver/ref/diver.ma";
requires maya "2008";
requires "Mayatomr" "9.0.1.4m - 3.6.51.0 ";
requires "COLLADA" "3.05B";
currentUnit -l centimeter -a degree -t ntsc;
fileInfo "application" "maya";
fileInfo "product" "Maya Unlimited 2008";
fileInfo "version" "2008 Service Pack 1";
fileInfo "cutIdentifier" "200802242333-718075";
fileInfo "osv" "Microsoft Windows XP Service Pack 2 (Build 2600)\n";
createNode transform -s -n "persp";
	setAttr ".v" no;
	setAttr ".t" -type "double3" -3.102574734671272 58.833103506273943 265.76357394412156 ;
	setAttr ".r" -type "double3" -2.7383527161620362 -0.19999999999993048 -1.2424117416441818e-017 ;
createNode camera -s -n "perspShape" -p "persp";
	setAttr -k off ".v" no;
	setAttr ".fl" 34.999999999999986;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 267.79943335244889;
	setAttr ".imn" -type "string" "persp";
	setAttr ".den" -type "string" "persp_depth";
	setAttr ".man" -type "string" "persp_mask";
	setAttr ".tp" -type "double3" -2.1688476876848384 46.038964640363851 -1.7284346624448972 ;
	setAttr ".hc" -type "string" "viewSet -p %camera";
createNode transform -s -n "top";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 5000.1000000000004 0 ;
	setAttr ".r" -type "double3" -89.999999999999986 0 0 ;
createNode camera -s -n "topShape" -p "top";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "top";
	setAttr ".den" -type "string" "top_depth";
	setAttr ".man" -type "string" "top_mask";
	setAttr ".hc" -type "string" "viewSet -t %camera";
	setAttr ".o" yes;
createNode transform -s -n "front";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 0 5000.1000000000004 ;
createNode camera -s -n "frontShape" -p "front";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "front";
	setAttr ".den" -type "string" "front_depth";
	setAttr ".man" -type "string" "front_mask";
	setAttr ".hc" -type "string" "viewSet -f %camera";
	setAttr ".o" yes;
createNode transform -s -n "side";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 5000.1000000000004 0 0 ;
	setAttr ".r" -type "double3" 0 89.999999999999986 0 ;
createNode camera -s -n "sideShape" -p "side";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "side";
	setAttr ".den" -type "string" "side_depth";
	setAttr ".man" -type "string" "side_mask";
	setAttr ".hc" -type "string" "viewSet -s %camera";
	setAttr ".o" yes;
createNode colladaDocument -n "colladaDocuments";
	setAttr ".doc[0].fn" -type "string" "";
createNode colladaDocument -n "colladaDocuments1";
	setAttr ".doc[0].fn" -type "string" "";
createNode colladaDocument -n "colladaDocuments2";
	setAttr ".doc[0].fn" -type "string" "";
createNode lightLinker -n "lightLinker1";
	setAttr -s 2 ".lnk";
	setAttr -s 2 ".slnk";
createNode displayLayerManager -n "layerManager";
	setAttr ".cdl" 4;
	setAttr -s 5 ".dli[1:4]"  1 2 3 4;
createNode displayLayer -n "defaultLayer";
createNode renderLayerManager -n "renderLayerManager";
createNode renderLayer -n "defaultRenderLayer";
	setAttr ".g" yes;
createNode reference -n "D_RN";
	setAttr -s 245 ".phl";
	setAttr ".phl[2318]" 0;
	setAttr ".phl[2319]" 0;
	setAttr ".phl[2320]" 0;
	setAttr ".phl[2321]" 0;
	setAttr ".phl[2322]" 0;
	setAttr ".phl[2323]" 0;
	setAttr ".phl[2324]" 0;
	setAttr ".phl[2325]" 0;
	setAttr ".phl[2326]" 0;
	setAttr ".phl[2327]" 0;
	setAttr ".phl[2328]" 0;
	setAttr ".phl[2329]" 0;
	setAttr ".phl[2330]" 0;
	setAttr ".phl[2331]" 0;
	setAttr ".phl[2332]" 0;
	setAttr ".phl[2333]" 0;
	setAttr ".phl[2334]" 0;
	setAttr ".phl[2335]" 0;
	setAttr ".phl[2336]" 0;
	setAttr ".phl[2337]" 0;
	setAttr ".phl[2338]" 0;
	setAttr ".phl[2339]" 0;
	setAttr ".phl[2340]" 0;
	setAttr ".phl[2341]" 0;
	setAttr ".phl[2342]" 0;
	setAttr ".phl[2343]" 0;
	setAttr ".phl[2344]" 0;
	setAttr ".phl[2345]" 0;
	setAttr ".phl[2346]" 0;
	setAttr ".phl[2347]" 0;
	setAttr ".phl[2348]" 0;
	setAttr ".phl[2349]" 0;
	setAttr ".phl[2350]" 0;
	setAttr ".phl[2351]" 0;
	setAttr ".phl[2352]" 0;
	setAttr ".phl[2353]" 0;
	setAttr ".phl[2354]" 0;
	setAttr ".phl[2355]" 0;
	setAttr ".phl[2356]" 0;
	setAttr ".phl[2357]" 0;
	setAttr ".phl[2358]" 0;
	setAttr ".phl[2359]" 0;
	setAttr ".phl[2360]" 0;
	setAttr ".phl[2361]" 0;
	setAttr ".phl[2362]" 0;
	setAttr ".phl[2363]" 0;
	setAttr ".phl[2364]" 0;
	setAttr ".phl[2365]" 0;
	setAttr ".phl[2366]" 0;
	setAttr ".phl[2367]" 0;
	setAttr ".phl[2368]" 0;
	setAttr ".phl[2369]" 0;
	setAttr ".phl[2370]" 0;
	setAttr ".phl[2371]" 0;
	setAttr ".phl[2372]" 0;
	setAttr ".phl[2373]" 0;
	setAttr ".phl[2374]" 0;
	setAttr ".phl[2375]" 0;
	setAttr ".phl[2376]" 0;
	setAttr ".phl[2377]" 0;
	setAttr ".phl[2378]" 0;
	setAttr ".phl[2379]" 0;
	setAttr ".phl[2380]" 0;
	setAttr ".phl[2381]" 0;
	setAttr ".phl[2382]" 0;
	setAttr ".phl[2383]" 0;
	setAttr ".phl[2384]" 0;
	setAttr ".phl[2385]" 0;
	setAttr ".phl[2386]" 0;
	setAttr ".phl[2387]" 0;
	setAttr ".phl[2388]" 0;
	setAttr ".phl[2389]" 0;
	setAttr ".phl[2390]" 0;
	setAttr ".phl[2391]" 0;
	setAttr ".phl[2392]" 0;
	setAttr ".phl[2393]" 0;
	setAttr ".phl[2394]" 0;
	setAttr ".phl[2395]" 0;
	setAttr ".phl[2396]" 0;
	setAttr ".phl[2397]" 0;
	setAttr ".phl[2398]" 0;
	setAttr ".phl[2399]" 0;
	setAttr ".phl[2400]" 0;
	setAttr ".phl[2401]" 0;
	setAttr ".phl[2402]" 0;
	setAttr ".phl[2403]" 0;
	setAttr ".phl[2404]" 0;
	setAttr ".phl[2405]" 0;
	setAttr ".phl[2406]" 0;
	setAttr ".phl[2407]" 0;
	setAttr ".phl[2408]" 0;
	setAttr ".phl[2409]" 0;
	setAttr ".phl[2410]" 0;
	setAttr ".phl[2411]" 0;
	setAttr ".phl[2412]" 0;
	setAttr ".phl[2413]" 0;
	setAttr ".phl[2414]" 0;
	setAttr ".phl[2415]" 0;
	setAttr ".phl[2416]" 0;
	setAttr ".phl[2417]" 0;
	setAttr ".phl[2418]" 0;
	setAttr ".phl[2419]" 0;
	setAttr ".phl[2420]" 0;
	setAttr ".phl[2421]" 0;
	setAttr ".phl[2422]" 0;
	setAttr ".phl[2423]" 0;
	setAttr ".phl[2424]" 0;
	setAttr ".phl[2425]" 0;
	setAttr ".phl[2426]" 0;
	setAttr ".phl[2427]" 0;
	setAttr ".phl[2428]" 0;
	setAttr ".phl[2429]" 0;
	setAttr ".phl[2430]" 0;
	setAttr ".phl[2431]" 0;
	setAttr ".phl[2432]" 0;
	setAttr ".phl[2433]" 0;
	setAttr ".phl[2434]" 0;
	setAttr ".phl[2435]" 0;
	setAttr ".phl[2436]" 0;
	setAttr ".phl[2437]" 0;
	setAttr ".phl[2438]" 0;
	setAttr ".phl[2439]" 0;
	setAttr ".phl[2440]" 0;
	setAttr ".phl[2441]" 0;
	setAttr ".phl[2442]" 0;
	setAttr ".phl[2443]" 0;
	setAttr ".phl[2444]" 0;
	setAttr ".phl[2445]" 0;
	setAttr ".phl[2446]" 0;
	setAttr ".phl[2447]" 0;
	setAttr ".phl[2448]" 0;
	setAttr ".phl[2449]" 0;
	setAttr ".phl[2450]" 0;
	setAttr ".phl[2451]" 0;
	setAttr ".phl[2452]" 0;
	setAttr ".phl[2453]" 0;
	setAttr ".phl[2454]" 0;
	setAttr ".phl[2455]" 0;
	setAttr ".phl[2456]" 0;
	setAttr ".phl[2457]" 0;
	setAttr ".phl[2458]" 0;
	setAttr ".phl[2459]" 0;
	setAttr ".phl[2460]" 0;
	setAttr ".phl[2461]" 0;
	setAttr ".phl[2462]" 0;
	setAttr ".phl[2463]" 0;
	setAttr ".phl[2464]" 0;
	setAttr ".phl[2465]" 0;
	setAttr ".phl[2466]" 0;
	setAttr ".phl[2467]" 0;
	setAttr ".phl[2468]" 0;
	setAttr ".phl[2469]" 0;
	setAttr ".phl[2470]" 0;
	setAttr ".phl[2471]" 0;
	setAttr ".phl[2472]" 0;
	setAttr ".phl[2473]" 0;
	setAttr ".phl[2474]" 0;
	setAttr ".phl[2475]" 0;
	setAttr ".phl[2476]" 0;
	setAttr ".phl[2477]" 0;
	setAttr ".phl[2478]" 0;
	setAttr ".phl[2479]" 0;
	setAttr ".phl[2480]" 0;
	setAttr ".phl[2481]" 0;
	setAttr ".phl[2482]" 0;
	setAttr ".phl[2483]" 0;
	setAttr ".phl[2484]" 0;
	setAttr ".phl[2485]" 0;
	setAttr ".phl[2486]" 0;
	setAttr ".phl[2487]" 0;
	setAttr ".phl[2488]" 0;
	setAttr ".phl[2489]" 0;
	setAttr ".phl[2490]" 0;
	setAttr ".phl[2491]" 0;
	setAttr ".phl[2492]" 0;
	setAttr ".phl[2493]" 0;
	setAttr ".phl[2494]" 0;
	setAttr ".phl[2495]" 0;
	setAttr ".phl[2496]" 0;
	setAttr ".phl[2497]" 0;
	setAttr ".phl[2498]" 0;
	setAttr ".phl[2499]" 0;
	setAttr ".phl[2500]" 0;
	setAttr ".phl[2501]" 0;
	setAttr ".phl[2502]" 0;
	setAttr ".phl[2503]" 0;
	setAttr ".phl[2504]" 0;
	setAttr ".phl[2505]" 0;
	setAttr ".phl[2506]" 0;
	setAttr ".phl[2507]" 0;
	setAttr ".phl[2508]" 0;
	setAttr ".phl[2509]" 0;
	setAttr ".phl[2510]" 0;
	setAttr ".phl[2511]" 0;
	setAttr ".phl[2512]" 0;
	setAttr ".phl[2513]" 0;
	setAttr ".phl[2514]" 0;
	setAttr ".phl[2515]" 0;
	setAttr ".phl[2516]" 0;
	setAttr ".phl[2517]" 0;
	setAttr ".phl[2518]" 0;
	setAttr ".phl[2519]" 0;
	setAttr ".phl[2520]" 0;
	setAttr ".phl[2521]" 0;
	setAttr ".phl[2522]" 0;
	setAttr ".phl[2523]" 0;
	setAttr ".phl[2524]" 0;
	setAttr ".phl[2525]" 0;
	setAttr ".phl[2526]" 0;
	setAttr ".phl[2527]" 0;
	setAttr ".phl[2528]" 0;
	setAttr ".phl[2529]" 0;
	setAttr ".phl[2530]" 0;
	setAttr ".phl[2531]" 0;
	setAttr ".phl[2532]" 0;
	setAttr ".phl[2533]" 0;
	setAttr ".phl[2534]" 0;
	setAttr ".phl[2535]" 0;
	setAttr ".phl[2536]" 0;
	setAttr ".phl[2537]" 0;
	setAttr ".phl[2538]" 0;
	setAttr ".phl[2539]" 0;
	setAttr ".phl[2540]" 0;
	setAttr ".phl[2541]" 0;
	setAttr ".phl[2542]" 0;
	setAttr ".phl[2543]" 0;
	setAttr ".phl[2544]" 0;
	setAttr ".phl[2545]" 0;
	setAttr ".phl[2546]" 0;
	setAttr ".phl[2547]" 0;
	setAttr ".phl[2548]" 0;
	setAttr ".phl[2549]" 0;
	setAttr ".phl[2550]" 0;
	setAttr ".phl[2551]" 0;
	setAttr ".phl[2552]" 0;
	setAttr ".phl[2553]" 0;
	setAttr ".phl[2554]" 0;
	setAttr ".phl[2555]" 0;
	setAttr ".phl[2556]" 0;
	setAttr ".phl[2557]" 0;
	setAttr ".phl[2558]" 0;
	setAttr ".phl[2559]" 0;
	setAttr ".ed" -type "dataReferenceEdits" 
		"D_RN"
		"D_RN" 3
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[2315]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[2316]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[2317]" ""
		"D_RN" 443
		2 "|D_:Entity" "translate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity" "translateX" " -av"
		2 "|D_:Entity" "translateY" " -av"
		2 "|D_:Entity" "translateZ" " -av"
		2 "|D_:Entity" "rotate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity" "rotateX" " -av"
		2 "|D_:Entity" "rotateY" " -av"
		2 "|D_:Entity" "rotateZ" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "translate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity|D_:DiverGlobal" "translateX" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "translateY" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "translateZ" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "rotate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity|D_:DiverGlobal" "rotateX" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "rotateY" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "rotateZ" " -av"
		2 "|D_:Diver|D_:DiverShape" "visibility" " -k 0 1"
		2 "|D_:Diver|D_:DiverShape" "intermediateObject" " 0"
		2 "|D_:Diver|D_:DiverShapeOrig" "visibility" " -k 0 1"
		2 "|D_:Diver|D_:DiverShapeOrig" "intermediateObject" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotate" " -type \"double3\" 0 0 -29.654061"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotate" " -type \"double3\" 0 0 -37.526539"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotate" " -type \"double3\" 0 0 -38.280067"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotate" " -type \"double3\" 0 0 -44.659082"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotate" " -type \"double3\" -4.493922 -0.794832 -23.031843"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotate" " -type \"double3\" 0 0 -34.510714"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotate" " -type \"double3\" -11.893513 -1.755435 -7.719601"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotate" " -type \"double3\" 0 0 0"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow" 
		"rotate" " -type \"double3\" -718.01436 -13.070252 1.506146"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotate" " -type \"double3\" 0 0 -27.828372"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotate" " -type \"double3\" 0 0 -37.329396"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotate" " -type \"double3\" 0 0 -48.302668"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotate" " -type \"double3\" 0 0 -40.679342"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotate" " -type \"double3\" 0 0 -16.176127"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotate" " -type \"double3\" 0 0 -46.14001"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotate" " -type \"double3\" -12.588356 -15.891604 -1.571825"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotate" " -type \"double3\" 0.0399682 0.102002 -0.328974"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"segmentScaleCompensate" " 1"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translate" " -type \"double3\" 2.995925 3.035394 37.041941"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotate" " -type \"double3\" -53.520932 18.719595 -51.816566"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "BallRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translate" " -type \"double3\" 2.932962 2.734056 34.298894"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotate" " -type \"double3\" -58.660132 -21.282589 57.460215"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "BallRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translate" " -type \"double3\" 22.917041 66.685344 1.681007"
		
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translate" " -type \"double3\" -48.557551 30.015589 -1.158412"
		
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translate" " -type \"double3\" -0.0228912 -31.003335 -0.651901"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotate" " -type \"double3\" -60.131756 1.532357 0.317559"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translate" " -type \"double3\" 0 0 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateX" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateY" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateZ" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotate" " -type \"double3\" 6.845435 -0.0366193 -0.132807"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateX" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateY" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateZ" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotate" " -type \"double3\" 6.400716 0.378008 -0.023713"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translate" " -type \"double3\" 0.00256438 -0.0454696 0.00556348"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotate" " -type \"double3\" -0.701781 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translate" " -type \"double3\" 0.00438577 0.779007 -0.00780013"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translate" " -type \"double3\" -0.000464093 0.69785 -0.013092"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translate" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateX" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateY" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateZ" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotate" " -type \"double3\" -28.649037 0.36477 0.423982"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"Mask" " -av -k 1 -1.534215"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translate" " -type \"double3\" 2.25911e-007 3.34534e-006 -2.31547e-006"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateX" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateY" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateZ" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotate" " -type \"double3\" -25.665134 26.20621 -25.414905"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotate" " -type \"double3\" -0.140524 4.791366 0.189755"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotate" " -type \"double3\" -88.998531 -30.528672 -25.171054"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotate" " -type \"double3\" -32.713115 -48.594161 20.83036"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotate" " -type \"double3\" -0.704304 13.206219 -1.760478"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotate" " -type \"double3\" -99.723796 38.548192 21.166926"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotate" " -type \"double3\" -7.427378 7.269141 0.144258"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateZ" " -av"
		2 "D_:leftControl" "visibility" " 1"
		2 "D_:joints" "displayType" " 0"
		2 "D_:joints" "visibility" " 0"
		2 "D_:geometry" "displayType" " 2"
		2 "D_:geometry" "visibility" " 1"
		2 "D_:rightControl" "visibility" " 1"
		2 "D_:torso" "visibility" " 1"
		2 "D_:skinCluster1" "nodeState" " 0"
		5 4 "D_RN" "|D_:Entity.translateX" "D_RN.placeHolderList[2318]" ""
		5 4 "D_RN" "|D_:Entity.translateY" "D_RN.placeHolderList[2319]" ""
		5 4 "D_RN" "|D_:Entity.translateZ" "D_RN.placeHolderList[2320]" ""
		5 4 "D_RN" "|D_:Entity.rotateX" "D_RN.placeHolderList[2321]" ""
		5 4 "D_RN" "|D_:Entity.rotateY" "D_RN.placeHolderList[2322]" ""
		5 4 "D_RN" "|D_:Entity.rotateZ" "D_RN.placeHolderList[2323]" ""
		5 4 "D_RN" "|D_:Entity.visibility" "D_RN.placeHolderList[2324]" ""
		5 4 "D_RN" "|D_:Entity.scaleX" "D_RN.placeHolderList[2325]" ""
		5 4 "D_RN" "|D_:Entity.scaleY" "D_RN.placeHolderList[2326]" ""
		5 4 "D_RN" "|D_:Entity.scaleZ" "D_RN.placeHolderList[2327]" ""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateX" "D_RN.placeHolderList[2328]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateY" "D_RN.placeHolderList[2329]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateZ" "D_RN.placeHolderList[2330]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateX" "D_RN.placeHolderList[2331]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateY" "D_RN.placeHolderList[2332]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateZ" "D_RN.placeHolderList[2333]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleX" "D_RN.placeHolderList[2334]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleY" "D_RN.placeHolderList[2335]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleZ" "D_RN.placeHolderList[2336]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.visibility" "D_RN.placeHolderList[2337]" 
		""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateX" 
		"D_RN.placeHolderList[2338]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateY" 
		"D_RN.placeHolderList[2339]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateZ" 
		"D_RN.placeHolderList[2340]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.visibility" 
		"D_RN.placeHolderList[2341]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateX" 
		"D_RN.placeHolderList[2342]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateY" 
		"D_RN.placeHolderList[2343]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateZ" 
		"D_RN.placeHolderList[2344]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.visibility" 
		"D_RN.placeHolderList[2345]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateX" 
		"D_RN.placeHolderList[2346]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateY" 
		"D_RN.placeHolderList[2347]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateZ" 
		"D_RN.placeHolderList[2348]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.visibility" 
		"D_RN.placeHolderList[2349]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateX" 
		"D_RN.placeHolderList[2350]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateY" 
		"D_RN.placeHolderList[2351]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateZ" 
		"D_RN.placeHolderList[2352]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.visibility" 
		"D_RN.placeHolderList[2353]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateX" 
		"D_RN.placeHolderList[2354]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateY" 
		"D_RN.placeHolderList[2355]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateZ" 
		"D_RN.placeHolderList[2356]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.visibility" 
		"D_RN.placeHolderList[2357]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateX" 
		"D_RN.placeHolderList[2358]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateY" 
		"D_RN.placeHolderList[2359]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateZ" 
		"D_RN.placeHolderList[2360]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.visibility" 
		"D_RN.placeHolderList[2361]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateX" 
		"D_RN.placeHolderList[2362]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateY" 
		"D_RN.placeHolderList[2363]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateZ" 
		"D_RN.placeHolderList[2364]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.visibility" 
		"D_RN.placeHolderList[2365]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateX" 
		"D_RN.placeHolderList[2366]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateY" 
		"D_RN.placeHolderList[2367]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateZ" 
		"D_RN.placeHolderList[2368]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.visibility" 
		"D_RN.placeHolderList[2369]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateX" 
		"D_RN.placeHolderList[2370]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateY" 
		"D_RN.placeHolderList[2371]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateZ" 
		"D_RN.placeHolderList[2372]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.visibility" 
		"D_RN.placeHolderList[2373]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateX" 
		"D_RN.placeHolderList[2374]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateY" 
		"D_RN.placeHolderList[2375]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateZ" 
		"D_RN.placeHolderList[2376]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.visibility" 
		"D_RN.placeHolderList[2377]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateX" 
		"D_RN.placeHolderList[2378]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateY" 
		"D_RN.placeHolderList[2379]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateZ" 
		"D_RN.placeHolderList[2380]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.visibility" 
		"D_RN.placeHolderList[2381]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateX" 
		"D_RN.placeHolderList[2382]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateY" 
		"D_RN.placeHolderList[2383]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateZ" 
		"D_RN.placeHolderList[2384]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.visibility" 
		"D_RN.placeHolderList[2385]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateX" 
		"D_RN.placeHolderList[2386]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateY" 
		"D_RN.placeHolderList[2387]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateZ" 
		"D_RN.placeHolderList[2388]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.visibility" 
		"D_RN.placeHolderList[2389]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateX" 
		"D_RN.placeHolderList[2390]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateY" 
		"D_RN.placeHolderList[2391]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateZ" 
		"D_RN.placeHolderList[2392]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.visibility" 
		"D_RN.placeHolderList[2393]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateX" 
		"D_RN.placeHolderList[2394]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateY" 
		"D_RN.placeHolderList[2395]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateZ" 
		"D_RN.placeHolderList[2396]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.visibility" 
		"D_RN.placeHolderList[2397]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateX" 
		"D_RN.placeHolderList[2398]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateY" 
		"D_RN.placeHolderList[2399]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateZ" 
		"D_RN.placeHolderList[2400]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.visibility" 
		"D_RN.placeHolderList[2401]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "D_RN.placeHolderList[2402]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.BallRoll" "D_RN.placeHolderList[2403]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateX" "D_RN.placeHolderList[2404]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateY" "D_RN.placeHolderList[2405]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateZ" "D_RN.placeHolderList[2406]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateX" "D_RN.placeHolderList[2407]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateY" "D_RN.placeHolderList[2408]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateZ" "D_RN.placeHolderList[2409]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleX" "D_RN.placeHolderList[2410]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleY" "D_RN.placeHolderList[2411]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleZ" "D_RN.placeHolderList[2412]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.visibility" "D_RN.placeHolderList[2413]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "D_RN.placeHolderList[2414]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.BallRoll" "D_RN.placeHolderList[2415]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateX" "D_RN.placeHolderList[2416]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateY" "D_RN.placeHolderList[2417]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateZ" "D_RN.placeHolderList[2418]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateX" "D_RN.placeHolderList[2419]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateY" "D_RN.placeHolderList[2420]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateZ" "D_RN.placeHolderList[2421]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleX" "D_RN.placeHolderList[2422]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleY" "D_RN.placeHolderList[2423]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleZ" "D_RN.placeHolderList[2424]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.visibility" "D_RN.placeHolderList[2425]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateX" "D_RN.placeHolderList[2426]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateY" "D_RN.placeHolderList[2427]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateZ" "D_RN.placeHolderList[2428]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleX" "D_RN.placeHolderList[2429]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleY" "D_RN.placeHolderList[2430]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleZ" "D_RN.placeHolderList[2431]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.visibility" "D_RN.placeHolderList[2432]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateX" "D_RN.placeHolderList[2433]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateY" "D_RN.placeHolderList[2434]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateZ" "D_RN.placeHolderList[2435]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleX" "D_RN.placeHolderList[2436]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleY" "D_RN.placeHolderList[2437]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleZ" "D_RN.placeHolderList[2438]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.visibility" "D_RN.placeHolderList[2439]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localPositionX" 
		"D_RN.placeHolderList[2440]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localPositionY" 
		"D_RN.placeHolderList[2441]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localPositionZ" 
		"D_RN.placeHolderList[2442]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localScaleX" 
		"D_RN.placeHolderList[2443]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localScaleY" 
		"D_RN.placeHolderList[2444]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localScaleZ" 
		"D_RN.placeHolderList[2445]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateX" "D_RN.placeHolderList[2446]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateY" "D_RN.placeHolderList[2447]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateZ" "D_RN.placeHolderList[2448]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateX" "D_RN.placeHolderList[2449]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateY" "D_RN.placeHolderList[2450]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateZ" "D_RN.placeHolderList[2451]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleX" "D_RN.placeHolderList[2452]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleY" "D_RN.placeHolderList[2453]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleZ" "D_RN.placeHolderList[2454]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.visibility" "D_RN.placeHolderList[2455]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateX" 
		"D_RN.placeHolderList[2456]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateY" 
		"D_RN.placeHolderList[2457]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateZ" 
		"D_RN.placeHolderList[2458]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleX" 
		"D_RN.placeHolderList[2459]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleY" 
		"D_RN.placeHolderList[2460]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleZ" 
		"D_RN.placeHolderList[2461]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateX" 
		"D_RN.placeHolderList[2462]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateY" 
		"D_RN.placeHolderList[2463]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateZ" 
		"D_RN.placeHolderList[2464]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.visibility" 
		"D_RN.placeHolderList[2465]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleX" 
		"D_RN.placeHolderList[2466]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleY" 
		"D_RN.placeHolderList[2467]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleZ" 
		"D_RN.placeHolderList[2468]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateX" 
		"D_RN.placeHolderList[2469]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateY" 
		"D_RN.placeHolderList[2470]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateZ" 
		"D_RN.placeHolderList[2471]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.visibility" 
		"D_RN.placeHolderList[2472]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleX" 
		"D_RN.placeHolderList[2473]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleY" 
		"D_RN.placeHolderList[2474]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleZ" 
		"D_RN.placeHolderList[2475]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateX" 
		"D_RN.placeHolderList[2476]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateY" 
		"D_RN.placeHolderList[2477]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateZ" 
		"D_RN.placeHolderList[2478]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateX" 
		"D_RN.placeHolderList[2479]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateY" 
		"D_RN.placeHolderList[2480]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateZ" 
		"D_RN.placeHolderList[2481]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.visibility" 
		"D_RN.placeHolderList[2482]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleX" 
		"D_RN.placeHolderList[2483]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleY" 
		"D_RN.placeHolderList[2484]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleZ" 
		"D_RN.placeHolderList[2485]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateX" 
		"D_RN.placeHolderList[2486]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateY" 
		"D_RN.placeHolderList[2487]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateZ" 
		"D_RN.placeHolderList[2488]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.visibility" 
		"D_RN.placeHolderList[2489]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleX" 
		"D_RN.placeHolderList[2490]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleY" 
		"D_RN.placeHolderList[2491]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleZ" 
		"D_RN.placeHolderList[2492]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateX" 
		"D_RN.placeHolderList[2493]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateY" 
		"D_RN.placeHolderList[2494]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateZ" 
		"D_RN.placeHolderList[2495]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.visibility" 
		"D_RN.placeHolderList[2496]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateX" 
		"D_RN.placeHolderList[2497]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateY" 
		"D_RN.placeHolderList[2498]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateZ" 
		"D_RN.placeHolderList[2499]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleX" 
		"D_RN.placeHolderList[2500]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleY" 
		"D_RN.placeHolderList[2501]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleZ" 
		"D_RN.placeHolderList[2502]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.Mask" 
		"D_RN.placeHolderList[2503]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateX" 
		"D_RN.placeHolderList[2504]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateY" 
		"D_RN.placeHolderList[2505]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateZ" 
		"D_RN.placeHolderList[2506]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.visibility" 
		"D_RN.placeHolderList[2507]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateX" 
		"D_RN.placeHolderList[2508]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateY" 
		"D_RN.placeHolderList[2509]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateZ" 
		"D_RN.placeHolderList[2510]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleX" 
		"D_RN.placeHolderList[2511]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleY" 
		"D_RN.placeHolderList[2512]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleZ" 
		"D_RN.placeHolderList[2513]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateX" 
		"D_RN.placeHolderList[2514]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateY" 
		"D_RN.placeHolderList[2515]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateZ" 
		"D_RN.placeHolderList[2516]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.visibility" 
		"D_RN.placeHolderList[2517]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleX" 
		"D_RN.placeHolderList[2518]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleY" 
		"D_RN.placeHolderList[2519]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleZ" 
		"D_RN.placeHolderList[2520]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateX" 
		"D_RN.placeHolderList[2521]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateY" 
		"D_RN.placeHolderList[2522]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateZ" 
		"D_RN.placeHolderList[2523]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.visibility" 
		"D_RN.placeHolderList[2524]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleX" 
		"D_RN.placeHolderList[2525]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleY" 
		"D_RN.placeHolderList[2526]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleZ" 
		"D_RN.placeHolderList[2527]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateX" 
		"D_RN.placeHolderList[2528]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateY" 
		"D_RN.placeHolderList[2529]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateZ" 
		"D_RN.placeHolderList[2530]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.visibility" 
		"D_RN.placeHolderList[2531]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleX" 
		"D_RN.placeHolderList[2532]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleY" 
		"D_RN.placeHolderList[2533]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleZ" 
		"D_RN.placeHolderList[2534]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateX" 
		"D_RN.placeHolderList[2535]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateY" 
		"D_RN.placeHolderList[2536]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateZ" 
		"D_RN.placeHolderList[2537]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.visibility" 
		"D_RN.placeHolderList[2538]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[2539]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[2540]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[2541]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[2542]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[2543]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[2544]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[2545]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleX" 
		"D_RN.placeHolderList[2546]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleY" 
		"D_RN.placeHolderList[2547]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleZ" 
		"D_RN.placeHolderList[2548]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateX" 
		"D_RN.placeHolderList[2549]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateY" 
		"D_RN.placeHolderList[2550]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateZ" 
		"D_RN.placeHolderList[2551]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.visibility" 
		"D_RN.placeHolderList[2552]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleX" 
		"D_RN.placeHolderList[2553]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleY" 
		"D_RN.placeHolderList[2554]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleZ" 
		"D_RN.placeHolderList[2555]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateX" 
		"D_RN.placeHolderList[2556]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateY" 
		"D_RN.placeHolderList[2557]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateZ" 
		"D_RN.placeHolderList[2558]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.visibility" 
		"D_RN.placeHolderList[2559]" "";
	setAttr ".ptag" -type "string" "";
lockNode;
createNode mentalrayItemsList -s -n "mentalrayItemsList";
createNode mentalrayGlobals -s -n "mentalrayGlobals";
createNode mentalrayOptions -s -n "miDefaultOptions";
	setAttr ".maxr" 2;
createNode mentalrayFramebuffer -s -n "miDefaultFramebuffer";
createNode script -n "uiConfigurationScriptNode";
	setAttr ".b" -type "string" (
		"// Maya Mel UI Configuration File.\n//\n//  This script is machine generated.  Edit at your own risk.\n//\n//\n\nglobal string $gMainPane;\nif (`paneLayout -exists $gMainPane`) {\n\n\tglobal int $gUseScenePanelConfig;\n\tint    $useSceneConfig = $gUseScenePanelConfig;\n\tint    $menusOkayInPanels = `optionVar -q allowMenusInPanels`;\tint    $nVisPanes = `paneLayout -q -nvp $gMainPane`;\n\tint    $nPanes = 0;\n\tstring $editorName;\n\tstring $panelName;\n\tstring $itemFilterName;\n\tstring $panelConfig;\n\n\t//\n\t//  get current state of the UI\n\t//\n\tsceneUIReplacement -update $gMainPane;\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Top View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Top View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"top\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"wireframe\" \n"
		+ "                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n"
		+ "                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n"
		+ "                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Top View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"top\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"wireframe\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n"
		+ "            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n"
		+ "            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n"
		+ "\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Side View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Side View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"side\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"wireframe\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n"
		+ "                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Side View\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"side\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"wireframe\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n"
		+ "            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Front View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Front View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"wireframe\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n"
		+ "                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n"
		+ "                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Front View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"wireframe\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n"
		+ "            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n"
		+ "            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Persp View\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Persp View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n"
		+ "                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n"
		+ "                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Persp View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n"
		+ "            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n"
		+ "            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n"
		+ "            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"outlinerPanel\" (localizedPanelLabel(\"Outliner\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\"Outliner\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            outlinerEditor -e \n                -showShapes 0\n                -showAttributes 0\n                -showConnected 0\n                -showAnimCurvesOnly 0\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 1\n                -ignoreDagHierarchy 0\n                -expandConnections 0\n                -showUnitlessCurves 1\n                -showCompounds 1\n"
		+ "                -showLeafs 1\n                -showNumericAttrsOnly 0\n                -highlightActive 1\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"defaultSetFilter\" \n                -showSetMembers 1\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\toutlinerPanel -edit -l (localizedPanelLabel(\"Outliner\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        outlinerEditor -e \n            -showShapes 0\n            -showAttributes 0\n            -showConnected 0\n            -showAnimCurvesOnly 0\n            -showMuteInfo 0\n            -autoExpand 0\n            -showDagOnly 1\n            -ignoreDagHierarchy 0\n            -expandConnections 0\n            -showUnitlessCurves 1\n            -showCompounds 1\n            -showLeafs 1\n            -showNumericAttrsOnly 0\n            -highlightActive 1\n            -autoSelectNewObjects 0\n            -doNotSelectNewObjects 0\n            -dropIsParent 1\n            -transmitFilters 0\n            -setFilter \"defaultSetFilter\" \n            -showSetMembers 1\n            -allowMultiSelection 1\n            -alwaysToggleSelect 0\n            -directSelect 0\n            -displayMode \"DAG\" \n            -expandObjects 0\n            -setsIgnoreFilters 1\n            -editAttrName 0\n            -showAttrValues 0\n            -highlightSecondary 0\n            -showUVAttrsOnly 0\n            -showTextureNodesOnly 0\n"
		+ "            -attrAlphaOrder \"default\" \n            -sortOrder \"none\" \n            -longNames 0\n            -niceNames 1\n            -showNamespace 1\n            $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"graphEditor\" (localizedPanelLabel(\"Graph Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"graphEditor\" -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 1\n                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n"
		+ "                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n            animCurveEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 1\n"
		+ "                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                -clipTime \"on\" \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 1\n                -showCompounds 0\n"
		+ "                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n            animCurveEditor -e \n                -displayKeys 1\n                -displayTangents 0\n"
		+ "                -displayActiveKeys 0\n                -displayActiveKeyTangents 1\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                -clipTime \"on\" \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\tif ($useSceneConfig) {\n\t\tscriptedPanel -e -to $panelName;\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dopeSheetPanel\" (localizedPanelLabel(\"Dope Sheet\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dopeSheetPanel\" -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n"
		+ "                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n"
		+ "                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n"
		+ "                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n"
		+ "                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\tif ($useSceneConfig) {\n\t\tscriptedPanel -e -to $panelName;\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"clipEditorPanel\" (localizedPanelLabel(\"Trax Editor\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"clipEditorPanel\" -l (localizedPanelLabel(\"Trax Editor\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = clipEditorNameFromPanel($panelName);\n            clipEditor -e \n                -displayKeys 0\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"none\" \n                -snapValue \"none\" \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Trax Editor\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = clipEditorNameFromPanel($panelName);\n            clipEditor -e \n                -displayKeys 0\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"none\" \n"
		+ "                -snapValue \"none\" \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"hyperGraphPanel\" (localizedPanelLabel(\"Hypergraph Hierarchy\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"hyperGraphPanel\" -l (localizedPanelLabel(\"Hypergraph Hierarchy\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"HyperGraphEd\");\n            hyperGraph -e \n                -graphLayoutStyle \"hierarchicalLayout\" \n                -orientation \"horiz\" \n                -zoom 1\n                -animateTransition 0\n                -showShapes 0\n                -showDeformers 0\n                -showExpressions 0\n                -showConstraints 0\n                -showUnderworld 0\n                -showInvisible 0\n                -transitionFrames 1\n                -freeform 0\n                -imagePosition 0 0 \n                -imageScale 1\n                -imageEnabled 0\n"
		+ "                -graphType \"DAG\" \n                -updateSelection 1\n                -updateNodeAdded 1\n                -useDrawOverrideColor 0\n                -limitGraphTraversal -1\n                -iconSize \"smallIcons\" \n                -showCachedConnections 0\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Hypergraph Hierarchy\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"HyperGraphEd\");\n            hyperGraph -e \n                -graphLayoutStyle \"hierarchicalLayout\" \n                -orientation \"horiz\" \n                -zoom 1\n                -animateTransition 0\n                -showShapes 0\n                -showDeformers 0\n                -showExpressions 0\n                -showConstraints 0\n                -showUnderworld 0\n                -showInvisible 0\n                -transitionFrames 1\n                -freeform 0\n                -imagePosition 0 0 \n                -imageScale 1\n                -imageEnabled 0\n"
		+ "                -graphType \"DAG\" \n                -updateSelection 1\n                -updateNodeAdded 1\n                -useDrawOverrideColor 0\n                -limitGraphTraversal -1\n                -iconSize \"smallIcons\" \n                -showCachedConnections 0\n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"hyperShadePanel\" (localizedPanelLabel(\"Hypershade\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"hyperShadePanel\" -l (localizedPanelLabel(\"Hypershade\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Hypershade\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"visorPanel\" (localizedPanelLabel(\"Visor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n"
		+ "\t\t\t$panelName = `scriptedPanel -unParent  -type \"visorPanel\" -l (localizedPanelLabel(\"Visor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Visor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"polyTexturePlacementPanel\" (localizedPanelLabel(\"UV Texture Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"polyTexturePlacementPanel\" -l (localizedPanelLabel(\"UV Texture Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"UV Texture Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"multiListerPanel\" (localizedPanelLabel(\"Multilister\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"multiListerPanel\" -l (localizedPanelLabel(\"Multilister\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Multilister\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"renderWindowPanel\" (localizedPanelLabel(\"Render View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"renderWindowPanel\" -l (localizedPanelLabel(\"Render View\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Render View\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"blendShapePanel\" (localizedPanelLabel(\"Blend Shape\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\tblendShapePanel -unParent -l (localizedPanelLabel(\"Blend Shape\")) -mbv $menusOkayInPanels ;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tblendShapePanel -edit -l (localizedPanelLabel(\"Blend Shape\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dynRelEdPanel\" (localizedPanelLabel(\"Dynamic Relationships\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dynRelEdPanel\" -l (localizedPanelLabel(\"Dynamic Relationships\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Dynamic Relationships\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"devicePanel\" (localizedPanelLabel(\"Devices\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n"
		+ "\t\t\tdevicePanel -unParent -l (localizedPanelLabel(\"Devices\")) -mbv $menusOkayInPanels ;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tdevicePanel -edit -l (localizedPanelLabel(\"Devices\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"relationshipPanel\" (localizedPanelLabel(\"Relationship Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"relationshipPanel\" -l (localizedPanelLabel(\"Relationship Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Relationship Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"referenceEditorPanel\" (localizedPanelLabel(\"Reference Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n"
		+ "\t\t\t$panelName = `scriptedPanel -unParent  -type \"referenceEditorPanel\" -l (localizedPanelLabel(\"Reference Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Reference Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"componentEditorPanel\" (localizedPanelLabel(\"Component Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"componentEditorPanel\" -l (localizedPanelLabel(\"Component Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Component Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dynPaintScriptedPanelType\" (localizedPanelLabel(\"Paint Effects\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dynPaintScriptedPanelType\" -l (localizedPanelLabel(\"Paint Effects\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Paint Effects\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"webBrowserPanel\" (localizedPanelLabel(\"Web Browser\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"webBrowserPanel\" -l (localizedPanelLabel(\"Web Browser\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Web Browser\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"scriptEditorPanel\" (localizedPanelLabel(\"Script Editor\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"scriptEditorPanel\" -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\tif ($useSceneConfig) {\n        string $configName = `getPanel -cwl (localizedPanelLabel(\"Current Layout\"))`;\n        if (\"\" != $configName) {\n\t\t\tpanelConfiguration -edit -label (localizedPanelLabel(\"Current Layout\")) \n\t\t\t\t-defaultImage \"\"\n\t\t\t\t-image \"\"\n\t\t\t\t-sc false\n\t\t\t\t-configString \"global string $gMainPane; paneLayout -e -cn \\\"vertical2\\\" -ps 1 20 100 -ps 2 80 100 $gMainPane;\"\n\t\t\t\t-removeAllPanels\n\t\t\t\t-ap false\n\t\t\t\t\t(localizedPanelLabel(\"Outliner\")) \n\t\t\t\t\t\"outlinerPanel\"\n\t\t\t\t\t\"$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\\\"Outliner\\\")) -mbv $menusOkayInPanels `;\\n$editorName = $panelName;\\noutlinerEditor -e \\n    -showShapes 0\\n    -showAttributes 0\\n    -showConnected 0\\n    -showAnimCurvesOnly 0\\n    -showMuteInfo 0\\n    -autoExpand 0\\n    -showDagOnly 1\\n    -ignoreDagHierarchy 0\\n    -expandConnections 0\\n    -showUnitlessCurves 1\\n    -showCompounds 1\\n    -showLeafs 1\\n    -showNumericAttrsOnly 0\\n    -highlightActive 1\\n    -autoSelectNewObjects 0\\n    -doNotSelectNewObjects 0\\n    -dropIsParent 1\\n    -transmitFilters 0\\n    -setFilter \\\"defaultSetFilter\\\" \\n    -showSetMembers 1\\n    -allowMultiSelection 1\\n    -alwaysToggleSelect 0\\n    -directSelect 0\\n    -displayMode \\\"DAG\\\" \\n    -expandObjects 0\\n    -setsIgnoreFilters 1\\n    -editAttrName 0\\n    -showAttrValues 0\\n    -highlightSecondary 0\\n    -showUVAttrsOnly 0\\n    -showTextureNodesOnly 0\\n    -attrAlphaOrder \\\"default\\\" \\n    -sortOrder \\\"none\\\" \\n    -longNames 0\\n    -niceNames 1\\n    -showNamespace 1\\n    $editorName\"\n"
		+ "\t\t\t\t\t\"outlinerPanel -edit -l (localizedPanelLabel(\\\"Outliner\\\")) -mbv $menusOkayInPanels  $panelName;\\n$editorName = $panelName;\\noutlinerEditor -e \\n    -showShapes 0\\n    -showAttributes 0\\n    -showConnected 0\\n    -showAnimCurvesOnly 0\\n    -showMuteInfo 0\\n    -autoExpand 0\\n    -showDagOnly 1\\n    -ignoreDagHierarchy 0\\n    -expandConnections 0\\n    -showUnitlessCurves 1\\n    -showCompounds 1\\n    -showLeafs 1\\n    -showNumericAttrsOnly 0\\n    -highlightActive 1\\n    -autoSelectNewObjects 0\\n    -doNotSelectNewObjects 0\\n    -dropIsParent 1\\n    -transmitFilters 0\\n    -setFilter \\\"defaultSetFilter\\\" \\n    -showSetMembers 1\\n    -allowMultiSelection 1\\n    -alwaysToggleSelect 0\\n    -directSelect 0\\n    -displayMode \\\"DAG\\\" \\n    -expandObjects 0\\n    -setsIgnoreFilters 1\\n    -editAttrName 0\\n    -showAttrValues 0\\n    -highlightSecondary 0\\n    -showUVAttrsOnly 0\\n    -showTextureNodesOnly 0\\n    -attrAlphaOrder \\\"default\\\" \\n    -sortOrder \\\"none\\\" \\n    -longNames 0\\n    -niceNames 1\\n    -showNamespace 1\\n    $editorName\"\n"
		+ "\t\t\t\t-ap false\n\t\t\t\t\t(localizedPanelLabel(\"Persp View\")) \n\t\t\t\t\t\"modelPanel\"\n"
		+ "\t\t\t\t\t\"$panelName = `modelPanel -unParent -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels `;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t\t\"modelPanel -edit -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels  $panelName;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t$configName;\n\n            setNamedPanelLayout (localizedPanelLabel(\"Current Layout\"));\n        }\n\n        panelHistory -e -clear mainPanelHistory;\n        setFocus `paneLayout -q -p1 $gMainPane`;\n        sceneUIReplacement -deleteRemaining;\n        sceneUIReplacement -clear;\n\t}\n\n\ngrid -spacing 500 -size 5000 -divisions 1 -displayAxes yes -displayGridLines yes -displayDivisionLines yes -displayPerspectiveLabels no -displayOrthographicLabels no -displayAxesBold yes -perspectiveLabelPosition axis -orthographicLabelPosition edge;\n}\n");
	setAttr ".st" 3;
createNode script -n "sceneConfigurationScriptNode";
	setAttr ".b" -type "string" "playbackOptions -min 40 -max 108 -ast 40 -aet 108 ";
	setAttr ".st" 6;
createNode animCurveTU -n "D_:RElbowFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 32 1 40 1 47 1 53 1 60 1;
	setAttr -s 6 ".kit[0:5]"  3 9 9 9 9 9;
	setAttr -s 6 ".kot[0:5]"  3 9 9 9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 32 1 40 1 47 1 53 1 60 1;
	setAttr -s 6 ".kit[0:5]"  3 9 9 9 9 9;
	setAttr -s 6 ".kot[0:5]"  3 9 9 9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 32 1 40 1 47 1 53 1 60 1;
	setAttr -s 6 ".kit[0:5]"  3 9 9 9 9 9;
	setAttr -s 6 ".kot[0:5]"  3 9 9 9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Entity_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTL -n "D_:Entity_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTL -n "D_:Entity_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTL -n "D_:Entity_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:Entity_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:Entity_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:Entity_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTU -n "D_:Entity_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:Entity_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:Entity_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:DiverGlobal_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTL -n "D_:DiverGlobal_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTL -n "D_:DiverGlobal_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 2.7755575615628914e-017 30 0 35 0;
createNode animCurveTL -n "D_:DiverGlobal_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:DiverGlobal_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:DiverGlobal_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:DiverGlobal_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTU -n "D_:DiverGlobal_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:DiverGlobal_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:DiverGlobal_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTL -n "D_:RootControl_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -0.606095;
createNode animCurveTL -n "D_:Spine0Control_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 0 2 0 4 0 6 0 10 0 11 0 15 0 19 0 21 
		0 23 0 30 0 32 0 40 0 41 0 47 0 49 0 53 0 54 0 57 0 59 0 60 0 65 0 79 0 87 0 89 0 
		104 0;
createNode animCurveTL -n "D_:HeadControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 0 2 0 4 0 6 0 10 0 11 0 15 0 19 0 21 
		0 23 0 30 0 32 0 40 0 41 0 47 0 49 0 53 0 54 0 57 0 59 0 60 0 65 0 79 0 87 0 89 0 
		104 0;
createNode animCurveTL -n "D_:LShoulderFK_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 0 2 0 4 0 6 -0.00086758995264350715 10 
		-0.00023316476668423737 11 0 15 -0.00086758995264350715 19 -0.00044999875026146435 
		21 0 23 -0.00086758995264350715 30 0 32 3.98341756740479e-006 40 3.98341756740479e-006 
		41 0 47 3.98341756740479e-006 49 3.98341756740479e-006 53 3.98341756740479e-006 54 
		3.98341756740479e-006 57 3.98341756740479e-006 59 3.98341756740479e-006 60 3.98341756740479e-006 
		65 3.98341756740479e-006 79 3.98341756740479e-006 87 3.98341756740479e-006 89 3.98341756740479e-006 
		104 3.98341756740479e-006;
createNode animCurveTL -n "D_:Spine0Control_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 -3.5527136788005009e-015 2 -3.5527136788005009e-015 
		4 -3.5527136788005009e-015 6 -3.5527136788005009e-015 10 -3.5527136788005009e-015 
		11 -3.5527136788005009e-015 15 -3.5527136788005009e-015 19 -3.5527136788005009e-015 
		21 -3.5527136788005009e-015 23 -3.5527136788005009e-015 30 -3.5527136788005009e-015 
		32 -3.5527136788005009e-015 40 -3.5527136788005009e-015 41 -3.5527136788005009e-015 
		47 -3.5527136788005009e-015 49 -3.5527136788005009e-015 53 -3.5527136788005009e-015 
		54 -3.5527136788005009e-015 57 -3.5527136788005009e-015 59 -3.5527136788005009e-015 
		60 -3.5527136788005009e-015 65 -3.5527136788005009e-015 79 -3.5527136788005009e-015 
		87 -3.5527136788005009e-015 89 -3.5527136788005009e-015 104 -3.5527136788005009e-015;
createNode animCurveTL -n "D_:HeadControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 0 2 0 4 0 6 0 10 0 11 0 15 0 19 0 21 
		0 23 0 30 0 32 0 40 0 41 0 47 0 49 0 53 0 54 0 57 0 59 0 60 0 65 0 79 0 87 0 89 0 
		104 0;
createNode animCurveTL -n "D_:LShoulderFK_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 0 2 0 4 0 6 -0.012847452797006685 10 
		-0.0034527525684767135 11 0 15 -0.012847452797006685 19 -0.0066636752112108717 21 
		0 23 -0.012847452797006685 30 0 32 5.8987275555905963e-005 40 5.8987275555905963e-005 
		41 0 47 5.8987275555905963e-005 49 5.8987275555905963e-005 53 5.8987275555905963e-005 
		54 5.8987275555905963e-005 57 5.8987275555905963e-005 59 5.8987275555905963e-005 
		60 5.8987275555905963e-005 65 5.8987275555905963e-005 79 5.8987275555905963e-005 
		87 5.8987275555905963e-005 89 5.8987275555905963e-005 104 5.8987275555905963e-005;
createNode animCurveTL -n "D_:Spine0Control_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 -1.1102230246251565e-016 2 -1.1102230246251565e-016 
		4 -1.1102230246251565e-016 6 -1.1102230246251565e-016 10 -1.1102230246251565e-016 
		11 -1.1102230246251565e-016 15 -1.1102230246251565e-016 19 -1.1102230246251565e-016 
		21 -1.1102230246251565e-016 23 -1.1102230246251565e-016 30 -1.1102230246251565e-016 
		32 -1.1102230246251565e-016 40 -1.1102230246251565e-016 41 -1.1102230246251565e-016 
		47 -1.1102230246251565e-016 49 -1.1102230246251565e-016 53 -1.1102230246251565e-016 
		54 -1.1102230246251565e-016 57 -1.1102230246251565e-016 59 -1.1102230246251565e-016 
		60 -1.1102230246251565e-016 65 -1.1102230246251565e-016 79 -1.1102230246251565e-016 
		87 -1.1102230246251565e-016 89 -1.1102230246251565e-016 104 -1.1102230246251565e-016;
createNode animCurveTL -n "D_:HeadControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 0 2 0 4 0 6 0 10 0 11 0 15 0 19 0 21 
		0 23 0 30 0 32 0 40 0 41 0 47 0 49 0 53 0 54 0 57 0 59 0 60 0 65 0 79 0 87 0 89 0 
		104 0;
createNode animCurveTL -n "D_:LShoulderFK_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 0 2 0 4 0 6 0.0088923674306018441 10 
		0.0023898234581872695 11 0 15 0.0088923674306018441 19 0.0046122643244648365 21 0 
		23 0.0088923674306018441 30 0 32 -4.0828057995653889e-005 40 -4.0828057995653889e-005 
		41 0 47 -4.0828057995653889e-005 49 -4.0828057995653889e-005 53 -4.0828057995653889e-005 
		54 -4.0828057995653889e-005 57 -4.0828057995653889e-005 59 -4.0828057995653889e-005 
		60 -4.0828057995653889e-005 65 -4.0828057995653889e-005 79 -4.0828057995653889e-005 
		87 -4.0828057995653889e-005 89 -4.0828057995653889e-005 104 -4.0828057995653889e-005;
createNode animCurveTU -n "D_:R_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 1 5 1 9 1 13 1 18 1 22 1 26 1 32 1 40 
		1 51 1 60 1 66 1 76 1 83 1 85 1 89 1 95 1 108 1;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTU -n "D_:HipControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:RootControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 2 1 6 1 10 1 15 1 19 1 23 1 32 1 40 
		1 63 1 69 1 76 1 89 1 98 1 108 1;
	setAttr -s 15 ".kit[7:14]"  3 3 9 9 9 1 3 3;
	setAttr -s 15 ".kot[7:14]"  3 3 9 9 9 1 3 3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTU -n "D_:Spine0Control_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:Spine1Control_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:HeadControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:TankControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:LElbowFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:L_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:RElbowFK_scaleX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 41 1 49 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 104 1;
createNode animCurveTU -n "D_:R_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:R_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  2 1 5 1 9 1 18 1 22 1 26 1 32 1 40 1 51 
		1 60 1 66 1 76 1 89 1 95 1 108 1;
	setAttr -s 15 ".kit[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kot[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTU -n "D_:L_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 54 1 63 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:L_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 54 1 63 1 67 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTU -n "D_:R_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 1 5 1 9 1 13 1 18 1 22 1 26 1 32 1 40 
		1 51 1 60 1 66 1 76 1 83 1 85 1 89 1 95 1 108 1;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTU -n "D_:HipControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1.0000000000000002 2 1.0000000000000002 
		4 1.0000000000000002 6 1.0000000000000002 10 1.0000000000000002 11 1.0000000000000002 
		15 1.0000000000000002 19 1.0000000000000002 21 1.0000000000000002 23 1.0000000000000002 
		30 1.0000000000000002 32 1.0000000000000002 40 1.0000000000000002 41 1.0000000000000002 
		47 1.0000000000000002 49 1.0000000000000002 53 1.0000000000000002 54 1.0000000000000002 
		57 1.0000000000000002 59 1.0000000000000002 60 1.0000000000000002 65 1.0000000000000002 
		79 1.0000000000000002 87 1.0000000000000002 89 1.0000000000000002 104 1.0000000000000002;
createNode animCurveTU -n "D_:RootControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 2 1 6 1 10 1 15 1 19 1 23 1 32 1 40 
		1 63 1 69 1 76 1 89 1 98 1 108 1;
	setAttr -s 15 ".kit[7:14]"  3 3 9 9 9 1 3 3;
	setAttr -s 15 ".kot[7:14]"  3 3 9 9 9 1 3 3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTU -n "D_:Spine0Control_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:Spine1Control_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:HeadControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:TankControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:LElbowFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:L_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:RElbowFK_scaleY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 41 1 49 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 104 1;
createNode animCurveTU -n "D_:R_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:R_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  2 1 5 1 9 1 18 1 22 1 26 1 32 1 40 1 51 
		1 60 1 66 1 76 1 89 1 95 1 108 1;
	setAttr -s 15 ".kit[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kot[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTU -n "D_:L_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 54 1 63 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:L_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 54 1 63 1 67 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTU -n "D_:R_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 1 5 1 9 1 13 1 18 1 22 1 26 1 32 1 40 
		1 51 1 60 1 66 1 76 1 83 1 85 1 89 1 95 1 108 1;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTU -n "D_:HipControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1.0000000000000002 2 1.0000000000000002 
		4 1.0000000000000002 6 1.0000000000000002 10 1.0000000000000002 11 1.0000000000000002 
		15 1.0000000000000002 19 1.0000000000000002 21 1.0000000000000002 23 1.0000000000000002 
		30 1.0000000000000002 32 1.0000000000000002 40 1.0000000000000002 41 1.0000000000000002 
		47 1.0000000000000002 49 1.0000000000000002 53 1.0000000000000002 54 1.0000000000000002 
		57 1.0000000000000002 59 1.0000000000000002 60 1.0000000000000002 65 1.0000000000000002 
		79 1.0000000000000002 87 1.0000000000000002 89 1.0000000000000002 104 1.0000000000000002;
createNode animCurveTU -n "D_:RootControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 2 1 6 1 10 1 15 1 19 1 23 1 32 1 40 
		1 63 1 69 1 76 1 89 1 98 1 108 1;
	setAttr -s 15 ".kit[7:14]"  3 3 9 9 9 1 3 3;
	setAttr -s 15 ".kot[7:14]"  3 3 9 9 9 1 3 3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTU -n "D_:Spine0Control_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:Spine1Control_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:HeadControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:TankControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:LElbowFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:L_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:RElbowFK_scaleZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 41 1 49 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 104 1;
createNode animCurveTU -n "D_:R_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 26 ".ktv[0:25]"  0 1 2 1 4 1 6 1 10 1 11 1 15 1 19 1 21 
		1 23 1 30 1 32 1 40 1 41 1 47 1 49 1 53 1 54 1 57 1 59 1 60 1 65 1 79 1 87 1 89 1 
		104 1;
createNode animCurveTU -n "D_:R_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  2 1 5 1 9 1 18 1 22 1 26 1 32 1 40 1 51 
		1 60 1 66 1 76 1 89 1 95 1 108 1;
	setAttr -s 15 ".kit[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kot[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTU -n "D_:L_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 54 1 63 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:L_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 54 1 63 1 67 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTU -n "D_:R_Foot_visibility";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 1 5 1 9 1 13 1 18 1 22 1 26 1 32 1 40 
		1 51 1 60 1 66 1 76 1 83 1 85 1 89 1 95 1 108 1;
	setAttr -s 18 ".kit[0:17]"  9 9 9 9 9 9 9 3 
		3 3 9 3 3 9 3 3 3 3;
	setAttr -s 18 ".kot[0:17]"  5 5 5 5 5 5 5 3 
		3 3 5 3 3 5 3 3 3 3;
createNode animCurveTU -n "D_:HipControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 6 1 10 1 15 1 19 1 23 1 32 1 40 
		1 54 1 63 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 16 ".kit[7:15]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 16 ".kot[0:15]"  5 5 5 5 5 5 5 3 
		3 3 5 5 5 1 3 3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
	setAttr -s 16 ".kox[13:15]"  1 1 1;
	setAttr -s 16 ".koy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:RootControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 6 1 10 1 15 1 19 1 23 1 32 1 40 
		1 59 1 63 1 69 1 76 1 89 1 98 1 108 1;
	setAttr -s 16 ".kit[7:15]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 16 ".kot[0:15]"  5 5 5 5 5 5 5 3 
		3 5 5 5 5 1 3 3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
	setAttr -s 16 ".kox[13:15]"  1 1 1;
	setAttr -s 16 ".koy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:Spine0Control_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 59 1 63 1 70 1 77 1 90 1 98 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 9 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 5 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:Spine1Control_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 4 1 8 1 12 1 17 1 21 1 25 1 32 
		1 40 1 59 1 63 1 71 1 78 1 91 1 100 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 9 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 5 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:HeadControl_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 1 2 1 5 1 9 1 13 1 18 1 22 1 26 1 32 
		1 40 1 54 1 59 1 63 1 72 1 79 1 92 1 101 1 108 1;
	setAttr -s 18 ".kit[0:17]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 9 1 3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 5 5 5 5 5 
		3 3;
	setAttr -s 18 ".kix[15:17]"  1 1 1;
	setAttr -s 18 ".kiy[15:17]"  0 0 0;
createNode animCurveTU -n "D_:TankControl_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 9 1 13 1 18 1 22 1 26 1 32 
		1 40 1 54 1 63 1 72 1 79 1 92 1 98 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:L_Clavicle_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 7 1 11 1 15 1 20 1 24 1 28 1 32 
		1 40 1 54 1 63 1 71 1 78 1 91 1 97 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:LShoulderFK_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 1 2 1 8 1 12 1 16 1 21 1 25 1 29 1 32 
		1 40 1 57 1 63 1 69 1 72 1 79 1 92 1 98 1 108 1;
	setAttr -s 18 ".kit[0:17]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 9 1 3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 5 5 5 5 5 
		3 3;
	setAttr -s 18 ".kix[15:17]"  1 1 1;
	setAttr -s 18 ".kiy[15:17]"  0 0 0;
createNode animCurveTU -n "D_:LElbowFK_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 52 1 57 1 63 1 73 1 80 1 93 1 99 1 108 1;
	setAttr -s 18 ".kit[0:17]"  9 9 9 9 9 9 9 9 
		3 3 9 3 9 9 9 1 3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 5 3 5 5 5 5 
		3 3;
	setAttr -s 18 ".kix[15:17]"  1 1 1;
	setAttr -s 18 ".kiy[15:17]"  0 0 0;
createNode animCurveTU -n "D_:L_Wrist_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 10 1 14 1 16 1 18 1 23 1 25 1 27 
		1 31 1 32 1 40 1 52 1 57 1 61 1 63 1 65 1 67 1 74 1 81 1 94 1 100 1 108 1;
	setAttr -s 23 ".kit[0:22]"  9 9 9 9 9 9 9 9 
		9 3 3 3 9 3 9 9 9 9 9 9 1 3 3;
	setAttr -s 23 ".kot[9:22]"  3 3 3 5 3 5 5 5 
		5 5 5 5 3 3;
	setAttr -s 23 ".kix[20:22]"  1 1 1;
	setAttr -s 23 ".kiy[20:22]"  0 0 0;
createNode animCurveTU -n "D_:l_thumb_1_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 11 1 15 1 19 1 24 1 28 1 32 1 40 
		1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 16 ".kit[0:15]"  9 9 9 9 9 9 9 3 
		3 3 9 9 9 1 3 3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:l_thumb_2_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 11 1 15 1 19 1 24 1 28 1 32 1 40 
		1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 16 ".kit[0:15]"  9 9 9 9 9 9 9 3 
		3 3 9 9 9 1 3 3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:l_point_1_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 11 1 15 1 19 1 24 1 28 1 32 1 40 
		1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 16 ".kit[0:15]"  9 9 9 9 9 9 9 3 
		3 3 9 9 9 1 3 3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:l_point_2_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 11 1 15 1 19 1 24 1 28 1 32 1 40 
		1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 16 ".kit[0:15]"  9 9 9 9 9 9 9 3 
		3 3 9 9 9 1 3 3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:l_mid_1_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 11 1 15 1 19 1 24 1 28 1 32 1 40 
		1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 16 ".kit[0:15]"  9 9 9 9 9 9 9 3 
		3 3 9 9 9 1 3 3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:l_mid_2_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 11 1 15 1 19 1 24 1 28 1 32 1 40 
		1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 16 ".kit[0:15]"  9 9 9 9 9 9 9 3 
		3 3 9 9 9 1 3 3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:l_pink_1_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 11 1 15 1 19 1 24 1 28 1 32 1 40 
		1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 16 ".kit[0:15]"  9 9 9 9 9 9 9 3 
		3 3 9 9 9 1 3 3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:l_pink_2_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 2 1 11 1 15 1 19 1 24 1 28 1 32 1 40 
		1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 16 ".kit[0:15]"  9 9 9 9 9 9 9 3 
		3 3 9 9 9 1 3 3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
createNode animCurveTU -n "D_:R_Clavicle_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 9 1 13 1 18 1 22 1 26 1 32 
		1 40 1 54 1 63 1 71 1 78 1 91 1 97 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:RShoulderFK_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 6 1 10 1 14 1 19 1 23 1 27 1 32 
		1 40 1 54 1 63 1 72 1 83 1 92 1 98 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:RElbowFK_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 7 1 11 1 15 1 20 1 24 1 28 1 32 
		1 40 1 54 1 63 1 73 1 84 1 93 1 99 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:R_Wrist_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 20 ".ktv[0:19]"  0 1 2 1 6 1 8 1 12 1 14 1 16 1 21 1 23 
		1 25 1 29 1 32 1 40 1 54 1 63 1 74 1 85 1 94 1 100 1 108 1;
	setAttr -s 20 ".kit[0:19]"  9 9 9 9 9 9 9 9 
		9 9 9 3 3 3 9 9 9 1 3 3;
	setAttr -s 20 ".kot[11:19]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 20 ".kix[17:19]"  1 1 1;
	setAttr -s 20 ".kiy[17:19]"  0 0 0;
createNode animCurveTU -n "D_:r_thumb_1_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:r_thumb_2_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:r_point_1_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:r_point_2_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:r_mid_1_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:r_mid_2_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:r_pink_1_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:r_pink_2_visibility";
	setAttr ".tan" 5;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 9 1 13 1 17 1 22 1 26 1 30 1 32 
		1 40 1 54 1 63 1 75 1 82 1 95 1 101 1 108 1;
	setAttr -s 17 ".kit[0:16]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 9 1 3 3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 5 5 5 5 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:R_Knee_visibility";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  2 1 5 1 9 1 18 1 22 1 26 1 32 1 40 1 51 
		1 60 1 66 1 76 1 89 1 95 1 108 1;
	setAttr -s 15 ".kit[0:14]"  9 9 9 9 9 9 3 3 
		3 9 9 9 1 3 3;
	setAttr -s 15 ".kot[0:14]"  5 5 5 5 5 5 3 3 
		3 5 5 5 1 3 3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTU -n "D_:L_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 54 1 63 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		3 3 3 5 5 5 1 3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTU -n "D_:L_Foot_visibility";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 1 2 1 3 1 7 1 11 1 16 1 20 1 24 1 32 
		1 40 1 54 1 63 1 67 1 69 1 76 1 89 1 95 1 108 1;
	setAttr -s 18 ".kit[0:17]"  9 9 9 9 9 9 9 9 
		3 3 3 9 9 3 3 3 3 3;
	setAttr -s 18 ".kot[0:17]"  5 5 5 5 5 5 5 5 
		3 3 3 5 5 3 3 3 3 3;
createNode animCurveTL -n "D_:R_KneeShape_localPositionX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 -8.3628095325263345;
createNode animCurveTL -n "D_:R_KneeShape_localPositionY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 24.703883880000003;
createNode animCurveTL -n "D_:R_KneeShape_localPositionZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 23.055145000000003;
createNode animCurveTL -n "D_:R_KneeShape_localScaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 1;
createNode animCurveTL -n "D_:R_KneeShape_localScaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 1;
createNode animCurveTL -n "D_:R_KneeShape_localScaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 1;
createNode animCurveTU -n "D_:HeadControl_Mask";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  8 50.7 12 0 16 39.2 21 0 25 8.6 29 0 59 
		-1.0078124912965891 72 -0.66183515663312031 79 -0.38504163895445115 92 0 101 0;
	setAttr -s 11 ".kit[9:10]"  1 9;
	setAttr -s 11 ".kot[9:10]"  1 9;
	setAttr -s 11 ".kix[9:10]"  1 1;
	setAttr -s 11 ".kiy[9:10]"  0 0;
	setAttr -s 11 ".kox[9:10]"  1 1;
	setAttr -s 11 ".koy[9:10]"  0 0;
createNode animCurveTU -n "D_:L_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 0 2 0 3 0 7 0 11 0 16 0 20 0 24 0 32 
		0 40 0 54 0 63 0 67 0 69 0 76 0 89 0 95 0 108 0;
	setAttr -s 18 ".kit[8:17]"  3 3 9 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 9 9 9 3 3 3 
		3 3;
createNode animCurveTU -n "D_:R_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 0 5 0 9 0 13 0 18 0 22 0 26 0 32 0 40 
		0 51 0 60 0 66 0 76 0 83 0 85 0 89 0 95 0 108 0;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTU -n "D_:L_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 0 2 0 3 0 7 0 11 0 16 0 20 0 24 0 32 
		0 40 0 54 0 63 0 67 0 69 0 76 0 89 0 95 0 108 0;
	setAttr -s 18 ".kit[8:17]"  3 3 9 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 9 9 9 3 3 3 
		3 3;
createNode animCurveTU -n "D_:R_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 0 5 0 9 0 13 0 18 0 22 0 26 0 32 0 40 
		0 51 0 60 0 66 0 76 0 83 0 85 0 89 0 95 0 108 0;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTL -n "D_:L_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 32.279062992200437 2 32.893980678211001 
		3 32.279062992200437 7 22.594109437639585 11 26.311088730336238 16 22.594109437639585 
		20 24.130857362698979 24 22.594109437639585 32 22.594109437639585 40 22.594109437639585 
		54 28.423878863057976 63 36.896833750720141 69 48.03692348902689 76 28.423878863057976 
		89 9.4513480500108429 95 9.4513480500108429 108 9.4513480500108429;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:L_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 2.9959092296987571 2 2.9959092296987571 
		3 2.9959092296987571 7 2.9959092296987571 11 3.2398709200391274 16 2.9959092296987571 
		20 3.8431751575801378 24 2.9959092296987571 32 2.9959092296987571 40 2.9959092296987571 
		54 2.9962019381999636 63 -5.433113518725988 67 0.81100724533385016 69 2.9964341932866638 
		76 2.9964341932866638 89 2.9964341932866638 95 2.9964341932866638 108 2.9964341932866638;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTL -n "D_:R_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 50.522608938728091 5 27.015919121585775 
		9 0.13987482996382994 13 8.7558812693195378 18 1.1716157713320143 22 4.8539631753799366 
		26 2.995084657595287 32 2.995084657595287 40 2.995084657595287 51 0 60 0 66 0 76 
		0 83 0 85 0 89 0 95 0 108 0;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTL -n "D_:RootControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -5.5464221608478859 2 -27.696583746122311 
		6 -31.036513367873908 10 -25.420773616093797 15 -31.036513367873908 19 -28.726399901327355 
		23 -31.036513367873908 32 -31.036513367873908 40 -31.036513367873908 63 -24.877450496516257 
		69 -13.059034438724566 76 -13.575065223344303 89 -9.0332870707853026 98 -5.5464221608478859 
		108 -5.5464221608478859;
	setAttr -s 15 ".kit[7:14]"  3 3 9 1 1 1 1 3;
	setAttr -s 15 ".kot[7:14]"  3 3 9 1 1 1 1 3;
	setAttr -s 15 ".kix[10:14]"  0.12651662528514862 0.5256270170211792 
		0.057722408324480057 0.68487769365310669 1;
	setAttr -s 15 ".kiy[10:14]"  0.99196451902389526 0.85071521997451782 
		0.998332679271698 0.72865808010101318 0;
	setAttr -s 15 ".kox[10:14]"  0.1265166699886322 0.52562671899795532 
		0.057722415775060654 0.68487769365310669 1;
	setAttr -s 15 ".koy[10:14]"  0.99196451902389526 0.85071533918380737 
		0.998332679271698 0.72865808010101318 0;
createNode animCurveTL -n "D_:TankControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -0.045469599999999999 2 -0.045469599999999999 
		5 -0.045469599999999999 9 -0.045469599999999999 13 0.21803612137654435 18 -0.045469599999999999 
		22 0.063969137789151503 26 -0.045469599999999999 32 -0.045469599999999999 40 -0.045469599999999999 
		54 -0.045469599999999999 63 -0.045469599999999999 72 -0.045469599999999999 79 -0.045469599999999999 
		92 -0.045469619462510075 98 -0.045469619462510075 108 -0.045469619462510075;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 3 3 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 3 3 3 
		3;
createNode animCurveTL -n "D_:R_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.69785 2 0.69785 5 0.69785 9 0.69785 
		13 0.69785 18 0.69785 22 0.69785 26 0.69785 32 0.69785 40 0.69785 54 0.69785 63 0.69785 
		71 0.69785 78 0.69785 91 0.6978503469639965 97 0.6978503469639965 108 0.6978503469639965;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:R_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  2 70.715486919989587 5 70.715486919989587 
		9 70.715486919989587 18 30.015588885700126 22 27.322213473129601 26 30.015588885700126 
		32 30.015588885700126 40 30.015588885700126 51 30.015588885700126 60 30.015588885700126 
		66 30.015588885700126 76 12.381430639526673 89 0 95 0 108 0;
	setAttr -s 15 ".kit[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kot[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTL -n "D_:L_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 70.715486919989587 2 70.715486919989587 
		3 70.715486919989587 7 70.715486919989587 11 70.715486919989587 16 70.715486919989587 
		20 70.715486919989587 24 70.715486919989587 32 70.715486919989587 40 70.715486919989587 
		54 -2.0393128469149922 63 -40.403562015613367 69 1.7530965209784206 76 -2.0393128469149922 
		89 0 95 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:L_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 40.010733637858976 2 23.453336776162214 
		3 16.50404382071666 7 -0.87945984651022258 11 7.2091794546457546 16 -0.87945984651022258 
		20 1.6965649795532736 24 3.1321336364758761 32 3.1321336364758761 40 3.1321336364758761 
		54 1.3857227168784465 63 8.0967255529181319 67 2.0580883909093473 69 0 76 0 89 0 
		95 0 108 0;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTL -n "D_:R_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 41.136712699604097 5 41.136712699604097 
		9 41.136712699604097 13 39.702632807459494 18 35.070173551134694 22 35.889625128608209 
		26 35.070173551134694 32 35.070173551134694 40 35.070173551134694 51 26.220390185909913 
		60 26.220390185909913 66 26.220390185909913 76 26.220390185909913 83 11.090879111775891 
		85 11.090879111775891 89 11.090879111775891 95 11.090879111775891 108 11.090879111775891;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTL -n "D_:RootControl_translateZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -0.606095 2 -0.606095 6 -0.606095 10 
		-0.606095 15 -0.606095 19 -0.606095 23 -0.606095 32 -0.606095 40 -0.606095 63 -0.9637564666115831 
		69 9.7152201278182382 76 4.5871481071014308 89 -0.60609539862080597 98 -0.60609539862080597 
		108 -0.60609539862080597;
	setAttr -s 15 ".kit[7:14]"  3 3 1 1 1 1 3 3;
	setAttr -s 15 ".kot[7:14]"  3 3 1 1 1 1 3 3;
	setAttr -s 15 ".kix[9:14]"  0.13738103210926056 0.54807305335998535 
		0.12399784475564957 0.20519548654556274 1 1;
	setAttr -s 15 ".kiy[9:14]"  0.99051827192306519 0.83643054962158203 
		-0.99228250980377197 -0.978721022605896 0 0;
	setAttr -s 15 ".kox[9:14]"  0.13738101720809937 0.54807311296463013 
		0.12399782985448837 0.20519542694091797 1 1;
	setAttr -s 15 ".koy[9:14]"  0.99051827192306519 0.83643049001693726 
		-0.99228250980377197 -0.978721022605896 0 0;
createNode animCurveTL -n "D_:TankControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.00556348 2 0.00556348 5 0.00556348 
		9 0.00556348 13 1.3636952210414313 18 0.00556348 22 0.45534269170346903 26 0.00556348 
		32 0.00556348 40 0.00556348 54 0.00556348 63 0.00556348 72 0.00556348 79 0.00556348 
		92 0.0055634826900936782 98 0.0055634826900936782 108 0.0055634826900936782;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 3 3 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 3 3 3 
		3;
createNode animCurveTL -n "D_:L_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -0.0078001299999999997 2 -0.0078001299999999997 
		7 -0.0078001299999999997 11 -0.0078001299999999997 15 -0.0078001299999999997 20 -0.0078001299999999997 
		24 -0.0078001299999999997 28 -0.0078001299999999997 32 -0.0078001299999999997 40 
		-0.0078001299999999997 54 -0.0078001299999999997 63 -0.0078001299999999997 71 -0.0078001299999999997 
		78 -0.0078001299999999997 91 -0.0078001293990800948 97 -0.0078001293990800948 108 
		-0.0078001293990800948;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:R_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -0.013092 2 -0.013092 5 -0.013092 9 -0.013092 
		13 -0.013092 18 -0.013092 22 -0.013092 26 -0.013092 32 -0.013092 40 -0.013092 54 
		-0.013092 63 -0.013092 71 -0.013092 78 -0.013092 91 -0.013092045525845527 97 -0.013092045525845527 
		108 -0.013092045525845527;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:L_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1.6810069718094027 2 1.6810069718094027 
		3 1.6810069718094027 7 1.6810069718094027 11 1.6810069718094027 16 1.6810069718094027 
		20 1.6810069718094027 24 1.6810069718094027 32 1.6810069718094027 40 1.6810069718094027 
		54 1.6810069718094027 63 -8.2602756734218516 69 -27.074224128507545 76 -34.345319720971638 
		89 0 95 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:R_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  2 -21.659810076259703 5 -21.659810076259703 
		9 6.5186188811778578 18 -1.1584119150679841 22 -1.6664506744064058 26 -1.1584119150679841 
		32 -1.1584119150679841 40 -1.1584119150679841 51 -1.1584119150679841 60 41.896808644881375 
		66 43.861204017343752 76 32.011540358642748 89 0 95 0 108 0;
	setAttr -s 15 ".kit[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kot[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTL -n "D_:L_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 46.162877802789652 2 46.39890673216717 
		3 46.162877802789652 7 42.445421918691181 11 41.006751006011406 16 42.445421918691181 
		20 37.887623170689906 24 37.61919391360825 32 37.61919391360825 40 37.61919391360825 
		54 27.198242818862475 63 20.035454650597536 67 2.5022024385758534 69 -2.448549867634112 
		76 -2.448549867634112 89 -2.448549867634112 95 -2.448549867634112 108 -2.448549867634112;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTA -n "D_:HeadControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 23.931978630215916 2 20.402897778371152 
		5 11.067367619159388 9 -29.538640174932738 13 1.9668485980125785 18 -29.538640174932738 
		22 -23.4798950219344 26 -29.538640174932738 32 -29.538640174932738 40 -29.538640174932738 
		54 -3.1474184289373999 59 9.8056547733028463 63 -0.79315905727788982 72 -15.616082222537955 
		79 -12.430760082569332 92 0.68002058101982232 101 0.68279051672447078 108 0.68228131693915994;
	setAttr -s 18 ".kit[8:17]"  3 3 1 9 9 1 1 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 1 9 9 1 1 1 
		3 3;
	setAttr -s 18 ".kix[10:17]"  0.6328125 0.99075025320053101 0.69870555400848389 
		0.9224933385848999 0.93380886316299438 0.98090708255767822 1 1;
	setAttr -s 18 ".kiy[10:17]"  0.77430516481399536 0.13569837808609009 
		-0.71540939807891846 -0.38601306080818176 -0.35777238011360168 0.19447706639766693 
		0 0;
	setAttr -s 18 ".kox[10:17]"  0.6328125 0.99075025320053101 0.69870555400848389 
		0.92249339818954468 0.93380880355834961 0.98090708255767822 1 1;
	setAttr -s 18 ".koy[10:17]"  0.77430510520935059 0.13569837808609009 
		-0.71540939807891846 -0.3860129714012146 -0.35777240991592407 0.19447711110115051 
		0 0;
createNode animCurveTA -n "D_:Spine1Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 13.799337746107811 2 11.797607222658783 
		4 9.853685343470115 8 6.3705016295424093 12 7.4221744008992525 17 6.3705016295424093 
		21 6.8053052266615772 25 6.3705016295424093 32 6.3705016295424093 40 6.3705016295424093 
		59 6.4072850253420253 63 2.82000601078866 71 10.43769017680248 78 6.665662477276701 
		91 1.877374678876925 100 3.7382994277233039 108 0.55216102146285018;
	setAttr -s 17 ".kit[8:16]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.99986439943313599 1 1;
	setAttr -s 17 ".kiy[14:16]"  -0.016469687223434448 0 0;
	setAttr -s 17 ".kox[14:16]"  0.99986439943313599 1 1;
	setAttr -s 17 ".koy[14:16]"  -0.016469787806272507 0 0;
createNode animCurveTA -n "D_:Spine0Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 17.103730177460317 2 11.877760912006144 
		3 9.853685343470115 7 6.3705016295424093 11 7.1833804730571273 16 6.3705016295424093 
		20 6.7065782596714181 24 6.3705016295424093 32 6.3705016295424093 40 6.3705016295424093 
		59 24.018075177741352 63 14.981485291619313 70 26.303052796990272 77 6.3705016295424093 
		90 2.9398009602852784 98 3.3265246475010537 108 0.14113566014355422;
	setAttr -s 17 ".kit[8:16]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.99886882305145264 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.047550614923238754 0 0;
	setAttr -s 17 ".kox[14:16]"  0.99886888265609741 1 1;
	setAttr -s 17 ".koy[14:16]"  0.047550875693559647 0 0;
createNode animCurveTA -n "D_:RootControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 -86.03049564358615 2 -68.051948931938711 
		6 -60.625450949398697 10 -66.752718094449648 15 -60.625450949398697 19 -63.118914373684319 
		23 -60.625450949398697 32 -60.625450949398697 40 -60.625450949398697 59 -24.989053455643077 
		63 13.584358539942793 69 15.228062251009144 76 21.063671081413808 89 4.6439713643199072 
		98 1.2910907900570192 108 1.2910907900570192;
	setAttr -s 16 ".kit[7:15]"  3 3 9 1 9 9 1 3 
		3;
	setAttr -s 16 ".kot[7:15]"  3 3 9 1 9 9 1 3 
		3;
	setAttr -s 16 ".kix[10:15]"  0.73030441999435425 0.95749807357788086 
		0.9636884331703186 0.91891276836395264 1 1;
	setAttr -s 16 ".kiy[10:15]"  0.68312191963195801 0.28843954205513 -0.26702922582626343 
		-0.39446085691452026 0 0;
	setAttr -s 16 ".kox[10:15]"  0.73030441999435425 0.95749807357788086 
		0.9636884331703186 0.91891276836395264 1 1;
	setAttr -s 16 ".koy[10:15]"  0.68312186002731323 0.28843954205513 -0.26702922582626343 
		-0.39446082711219788 0 0;
createNode animCurveTA -n "D_:HipControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 -7.955963018273553 2 -7.955963018273553 
		6 -7.955963018273553 10 -7.955963018273553 15 -7.955963018273553 19 -7.955963018273553 
		23 -7.955963018273553 32 -7.955963018273553 40 -7.955963018273553 54 1.5864047080801922 
		63 1.5864047080801922 69 1.5864047080801922 76 1.5864047080801922 89 0 95 0 108 0;
	setAttr -s 16 ".kit[7:15]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
	setAttr -s 16 ".kox[13:15]"  1 1 1;
	setAttr -s 16 ".koy[13:15]"  0 0 0;
createNode animCurveTA -n "D_:R_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 -92.142317270095774 5 -92.142317270095774 
		9 -39.893734705007475 13 -79.429646904823059 18 -47.103970911780529 22 -60.805102510784081 
		26 -64.260593957478363 32 -64.260593957478363 40 -64.260593957478363 51 0 60 0 66 
		0 76 0 83 -9.2841095231390725 85 0 89 0 95 0 108 0;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTL -n "D_:R_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  2 -27.884307763405538 5 -27.884307763405538 
		9 -47.979899062356225 18 -53.146302994186925 22 -53.488197359491394 26 -53.146302994186925 
		32 -53.146302994186925 40 -53.146302994186925 51 -0.49423667655099024 60 -26.869586241525759 
		66 -0.49423667655099024 76 -4.4675054951837883 89 -11.18549088762272 95 -6.915388563106255 
		108 -6.915388563106255;
	setAttr -s 15 ".kit[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kot[6:14]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 15 ".kix[12:14]"  1 1 1;
	setAttr -s 15 ".kiy[12:14]"  0 0 0;
	setAttr -s 15 ".kox[12:14]"  1 1 1;
	setAttr -s 15 ".koy[12:14]"  0 0 0;
createNode animCurveTL -n "D_:R_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -0.000464093 2 -0.000464093 5 -0.000464093 
		9 -0.000464093 13 -0.000464093 18 -0.000464093 22 -0.000464093 26 -0.000464093 32 
		-0.000464093 40 -0.000464093 54 -0.000464093 63 -0.000464093 71 -0.000464093 78 -0.000464093 
		91 -0.0004640926137400277 97 -0.0004640926137400277 108 -0.0004640926137400277;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:TankControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.0025643799999999998 2 0.0025643799999999998 
		5 0.0025643799999999998 9 0.0025643799999999998 13 0.074943845691361588 18 0.0025643799999999998 
		22 0.0201378648930092 26 0.0025643799999999998 32 0.0025643799999999998 40 0.0025643799999999998 
		54 0.0025643799999999998 63 0.0025643799999999998 72 0.0025643799999999998 79 0.0025643799999999998 
		92 0.0025643796042819182 98 0.0025643796042819182 108 0.0025643796042819182;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 3 3 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 3 3 3 
		3;
createNode animCurveTL -n "D_:L_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.779007 2 0.779007 7 0.779007 11 0.779007 
		15 0.779007 20 0.779007 24 0.779007 28 0.779007 32 0.779007 40 0.779007 54 0.779007 
		63 0.779007 71 0.779007 78 0.779007 91 0.77900741554026665 97 0.77900741554026665 
		108 0.77900741554026665;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:L_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.0043857699999999998 2 0.0043857699999999998 
		7 0.0043857699999999998 11 0.0043857699999999998 15 0.0043857699999999998 20 0.0043857699999999998 
		24 0.0043857699999999998 28 0.0043857699999999998 32 0.0043857699999999998 40 0.0043857699999999998 
		54 0.0043857699999999998 63 0.0043857699999999998 71 0.0043857699999999998 78 0.0043857699999999998 
		91 0.0043857699112451395 97 0.0043857699112451395 108 0.0043857699112451395;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTL -n "D_:R_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 -4.2655129596276726 5 -4.2655129596276726 
		9 2.9329621506256558 13 0.17026614182016564 18 2.9329621506256558 22 2.3818226389570509 
		26 2.9329621506256558 32 2.9329621506256558 40 2.9329621506256558 51 2.9329621506256558 
		60 2.9329621506256558 66 2.9329621506256558 76 2.9329621506256558 83 -4.2649879960397659 
		85 -4.2649879960397659 89 -4.2649879960397659 95 -4.2649879960397659 108 -4.2649879960397659;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTL -n "D_:RootControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 2 0 6 0 10 0 15 0 19 0 23 0 32 0 40 
		0 63 -0.59171626817060907 69 1.8713574790161545 76 8.5917177152303736 89 -3.8585644858441732 
		98 -3.533378648420376 108 0;
	setAttr -s 15 ".kit[7:14]"  3 3 9 9 9 1 1 3;
	setAttr -s 15 ".kot[7:14]"  3 3 9 9 9 1 1 3;
	setAttr -s 15 ".kix[12:14]"  1 0.23868533968925476 1;
	setAttr -s 15 ".kiy[12:14]"  0 0.97109699249267578 0;
	setAttr -s 15 ".kox[12:14]"  1 0.23868536949157715 1;
	setAttr -s 15 ".koy[12:14]"  0 0.97109699249267578 0;
createNode animCurveTA -n "D_:l_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:l_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:l_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:l_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:l_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -0.794832 2 -0.794832 11 -0.794832 15 
		-0.20606727481080217 19 -0.794832 24 -0.28579596311051675 28 -0.794832 32 -0.64349506463808748 
		40 -0.794832 54 -0.794832 63 0 68 -0.794832 75 -0.794832 82 -0.794832 95 -3.173264837453968 
		101 -3.173264837453968 108 -3.173264837453968;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.96828126907348633 1 1;
	setAttr -s 17 ".kiy[14:16]"  -0.24986264109611511 0 0;
	setAttr -s 17 ".kox[14:16]"  0.96828126907348633 1 1;
	setAttr -s 17 ".koy[14:16]"  -0.24986264109611511 0 0;
createNode animCurveTA -n "D_:l_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.93561077117919922 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.35303330421447754 0 0;
	setAttr -s 17 ".kox[14:16]"  0.93561077117919922 1 1;
	setAttr -s 17 ".koy[14:16]"  0.35303330421447754 0 0;
createNode animCurveTA -n "D_:l_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1.755435 2 -1.755435 11 -1.755435 15 
		-2.2588605156110289 19 -1.755435 24 -2.1906881985197284 28 -1.755435 32 -1.8848362224669768 
		40 -1.755435 54 -1.755435 63 -2.4350591220048674 68 -1.755435 75 -1.755435 82 -1.755435 
		95 -18.366743592299766 101 -18.366743592299766 108 -18.366743592299766;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.9234582781791687 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.38369891047477722 0 0;
	setAttr -s 17 ".kox[14:16]"  0.9234582781791687 1 1;
	setAttr -s 17 ".koy[14:16]"  0.38369891047477722 0 0;
createNode animCurveTA -n "D_:L_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 4.0808238580019598 2 5.009507867717625 
		10 -16.757981363675544 14 -30.844327868686506 16 10.937759331895412 18 -8.5300638478878383 
		23 -30.844327868686506 25 -11.833028805209052 27 -28.013393447575837 31 -30.844327868686506 
		32 -30.844327868686506 40 -30.844327868686506 52 -21.958917754364414 57 -9.8819504576106958 
		61 -37.179398888692646 63 -47.669053285797979 65 -32.686608950893657 67 -3.1953281459452576 
		74 -2.1924329375381144 81 -1.7426198502578432 94 -8.1070161891185535 100 -8.1070161891185535 
		108 -8.1070161891185535;
	setAttr -s 23 ".kit[9:22]"  3 3 3 9 3 9 9 9 
		9 9 9 1 3 3;
	setAttr -s 23 ".kot[9:22]"  3 3 3 9 3 9 9 9 
		9 9 9 1 3 3;
	setAttr -s 23 ".kix[20:22]"  0.66511493921279907 1 1;
	setAttr -s 23 ".kiy[20:22]"  0.74674093723297119 0 0;
	setAttr -s 23 ".kox[20:22]"  0.66511493921279907 1 1;
	setAttr -s 23 ".koy[20:22]"  0.74674093723297119 0 0;
createNode animCurveTA -n "D_:LElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 -10.773467821346996 2 -11.341452327000416 
		9 -51.931596209449374 13 7.4020349556572906 17 -19.036349389954694 22 7.4020349556572906 
		26 -3.8898807682999852 30 7.4020349556572906 32 7.4020349556572906 40 7.4020349556572906 
		52 -35.256524715263104 57 -26.210193422137237 63 1.8480043990996418 73 -20.132665688365105 
		80 -20.132665688365105 93 -32.439136911498778 99 -32.439136911498778 108 -32.439136911498778;
	setAttr -s 18 ".kit[8:17]"  3 3 9 3 9 9 9 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 9 3 9 9 9 1 
		3 3;
	setAttr -s 18 ".kix[15:17]"  0.62282228469848633 1 1;
	setAttr -s 18 ".kiy[15:17]"  0.78236335515975952 0 0;
	setAttr -s 18 ".kox[15:17]"  0.62282228469848633 1 1;
	setAttr -s 18 ".koy[15:17]"  0.78236335515975952 0 0;
createNode animCurveTA -n "D_:LShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 -69.54968125454505 2 -73.168246748968258 
		8 -47.68726040621187 12 27.970644970134352 16 21.052104125551352 21 27.970644970134352 
		25 26.590636729495234 29 27.970644970134352 32 27.970644970134352 40 27.970644970134352 
		57 -18.139383833268063 63 -26.786957891799076 69 -35.766257649802405 72 -28.592595408181168 
		79 -1.1357915801763319 92 0 98 0 108 0;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 9 1 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 9 1 1 
		3 3;
	setAttr -s 18 ".kix[14:17]"  0.96635144948959351 0.99966007471084595 
		1 1;
	setAttr -s 18 ".kiy[14:17]"  0.25722548365592957 0.026073090732097626 
		0 0;
	setAttr -s 18 ".kox[14:17]"  0.96635144948959351 0.99966007471084595 
		1 1;
	setAttr -s 18 ".koy[14:17]"  0.25722557306289673 0.026072340086102486 
		0 0;
createNode animCurveTA -n "D_:TankControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 9 0 13 0 18 0 22 0 26 0 32 
		0 40 0 54 0 63 0 72 0 79 0 92 0 98 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 3 3 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 3 3 3 
		3;
createNode animCurveTA -n "D_:HeadControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 -0.026536300000000075 2 -0.026536300000000054 
		5 -0.026536300000000016 9 -0.02653630000000003 13 -0.026536300000000027 18 -0.02653630000000003 
		22 -0.026536300000000027 26 -0.02653630000000003 32 -0.02653630000000003 40 -0.02653630000000003 
		54 9.9165556412839972 59 12.096110769687655 63 21.403638350791965 72 -4.8977802936383759 
		79 -26.930166869014279 92 -0.061515252111021837 101 0.0030082999790115496 108 -0.026536333767640648;
	setAttr -s 18 ".kit[8:17]"  3 3 1 1 9 9 9 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 1 1 9 9 9 1 
		3 3;
	setAttr -s 18 ".kix[10:17]"  0.94649392366409302 0.94164758920669556 
		0.8252112865447998 0.53438234329223633 0.99207967519760132 0.99997478723526001 1 
		1;
	setAttr -s 18 ".kiy[10:17]"  0.32272174954414368 0.33660036325454712 
		-0.56482404470443726 -0.84524291753768921 0.12561033666133881 0.0071061393246054649 
		0 0;
	setAttr -s 18 ".kox[10:17]"  0.94649392366409302 0.94164764881134033 
		0.8252112865447998 0.53438234329223633 0.99207967519760132 0.99997478723526001 1 
		1;
	setAttr -s 18 ".koy[10:17]"  0.32272160053253174 0.33660033345222473 
		-0.56482404470443726 -0.84524291753768921 0.12561033666133881 0.0071060461923480034 
		0 0;
createNode animCurveTA -n "D_:Spine1Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1.9114352785588817 2 -0.83625293676933532 
		4 0 8 0 12 0.13813106207525966 17 0 21 0.057108906219461235 25 0 32 0 40 0 59 17.471026213175797 
		63 19.779534484968661 71 14.08834426073942 78 17.077954352455645 91 -7.6013550200003817 
		100 -3.3036041709326089 108 0.0084151252342212438;
	setAttr -s 17 ".kit[8:16]"  3 3 9 9 9 1 1 1 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 9 9 9 1 1 1 
		3;
	setAttr -s 17 ".kix[13:16]"  0.99993634223937988 0.99866080284118652 
		0.95025169849395752 1;
	setAttr -s 17 ".kiy[13:16]"  -0.011283612810075283 -0.051737286150455475 
		0.31148302555084229 0;
	setAttr -s 17 ".kox[13:16]"  0.99993634223937988 0.99866080284118652 
		0.95025175809860229 1;
	setAttr -s 17 ".koy[13:16]"  -0.011283593252301216 -0.051737282425165176 
		0.31148302555084229 0;
createNode animCurveTA -n "D_:Spine0Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 3 0 7 0 11 0 16 0 20 0 24 0 32 
		0 40 0 59 -2.2765023780462026 63 -4.1201723659299843 70 -9.537301396156181 77 0 90 
		1.1612023246373344 98 -3.3078461675582682 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.99779462814331055 1 1;
	setAttr -s 17 ".kiy[14:16]"  -0.066377222537994385 0 0;
	setAttr -s 17 ".kox[14:16]"  0.99779462814331055 1 1;
	setAttr -s 17 ".koy[14:16]"  -0.066377222537994385 0 0;
createNode animCurveTA -n "D_:RootControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 3.1781889865140549 2 3.1781889865140553 
		6 1.239820085060547 10 3.1781889865140553 15 1.239820085060547 19 1.9193691470446623 
		23 1.239820085060547 32 1.239820085060547 40 1.239820085060547 59 17.680143833765182 
		63 27.565804410579897 69 52.296070009219285 76 37.305733499133119 89 0 98 0 108 0;
	setAttr -s 16 ".kit[7:15]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 16 ".kot[7:15]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
	setAttr -s 16 ".kox[13:15]"  1 1 1;
	setAttr -s 16 ".koy[13:15]"  0 0 0;
createNode animCurveTA -n "D_:HipControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 7.2798116407249385 2 7.2798116407249385 
		6 7.2798116407249385 10 7.2798116407249385 15 7.2798116407249385 19 7.2798116407249385 
		23 7.2798116407249385 32 7.2798116407249385 40 7.2798116407249385 54 7.087179427603588 
		63 7.087179427603588 69 7.087179427603588 76 7.087179427603588 89 7.2798116407249278 
		95 7.2798116407249278 108 7.2798116407249278;
	setAttr -s 16 ".kit[7:15]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
	setAttr -s 16 ".kox[13:15]"  1 1 1;
	setAttr -s 16 ".koy[13:15]"  0 0 0;
createNode animCurveTA -n "D_:R_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 -0.9456590963667092 5 -0.9456590963667092 
		9 -4.5919795683795686 13 -12.663708628436973 18 -29.229969910608542 22 -4.5747799845786847 
		26 -23.314502742108239 32 -23.314502742108239 40 -23.314502742108239 51 0 60 0 66 
		0 76 0 83 -28.591340383919473 85 -29.070586091195047 89 -29.070586091195047 95 -29.070586091195047 
		108 -29.070586091195047;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTA -n "D_:TankControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -0.701781 2 -0.701781 5 -0.701781 9 -0.701781 
		13 -0.701781 18 -0.701781 22 -0.701781 26 -0.701781 32 -0.701781 40 -0.701781 54 
		-0.701781 63 -0.701781 72 -0.701781 79 -0.701781 92 -0.70178127080170383 98 -0.70178127080170383 
		108 -0.70178127080170383;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 3 3 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 3 3 3 
		3;
createNode animCurveTA -n "D_:LShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 -38.413995681368256 2 -38.858408037951584 
		8 -12.878902204132139 12 -26.437083662190155 16 -19.11053225713891 21 -26.437083662190155 
		25 -26.887127132400757 29 -26.437083662190155 32 -26.437083662190155 40 -26.437083662190155 
		57 -6.2637074461705593 63 -55.668174234413669 69 -23.304889545521061 72 -12.769577430805857 
		79 -38.244211238914701 92 0 98 0 108 0;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 18 ".kix[15:17]"  0.99926429986953735 1 1;
	setAttr -s 18 ".kiy[15:17]"  0.038352526724338531 0 0;
	setAttr -s 18 ".kox[15:17]"  0.99926429986953735 1 1;
	setAttr -s 18 ".koy[15:17]"  0.038352396339178085 0 0;
createNode animCurveTA -n "D_:LElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 52 -2.5811473339900934 57 -3.1011287820014366 63 0 73 0 80 0 93 0 99 0 108 
		0;
	setAttr -s 18 ".kit[8:17]"  3 3 9 3 9 9 9 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 9 3 9 9 9 1 
		3 3;
	setAttr -s 18 ".kix[15:17]"  1 1 1;
	setAttr -s 18 ".kiy[15:17]"  0 0 0;
	setAttr -s 18 ".kox[15:17]"  1 1 1;
	setAttr -s 18 ".koy[15:17]"  0 0 0;
createNode animCurveTA -n "D_:L_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -37.163962980744763 2 -35.62708377745826 
		10 -45.606688528262566 14 -94.961599170900712 16 -82.917286553101306 18 -67.565630360176897 
		23 -94.961599170900712 25 -92.77406348221173 27 -88.96447525406532 31 -94.961599170900712 
		32 -94.961599170900712 40 -94.961599170900712 52 13.50054353161288 57 31.793060444596261 
		61 34.316870682412763 63 41.698036674782465 65 46.630511253650212 67 47.325448894650123 
		74 13.181657332313231 81 21.655885558874008 94 1.7208864831813682 100 1.7208864831813682 
		108 1.7208864831813682;
	setAttr -s 23 ".kit[9:22]"  3 3 3 9 3 9 9 9 
		9 9 9 1 3 3;
	setAttr -s 23 ".kot[9:22]"  3 3 3 9 3 9 9 9 
		9 9 9 1 3 3;
	setAttr -s 23 ".kix[20:22]"  0.66747474670410156 1 1;
	setAttr -s 23 ".kiy[20:22]"  -0.74463242292404175 0 0;
	setAttr -s 23 ".kox[20:22]"  0.66747474670410156 1 1;
	setAttr -s 23 ".koy[20:22]"  -0.74463242292404175 0 0;
createNode animCurveTA -n "D_:l_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -11.893513 2 -11.893513 11 -11.893513 
		15 -3.1277004565724522 19 -11.893513 24 -4.314739511343979 28 -11.893513 32 -9.6403358796196272 
		40 -11.893513 54 -11.893513 63 -0.05967170918783523 68 -11.893513 75 -11.893513 82 
		-11.893513 95 -18.948084576790443 101 -18.948084576790443 108 -18.948084576790443;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.62793123722076416 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.77826881408691406 0 0;
	setAttr -s 17 ".kox[14:16]"  0.62793123722076416 1 1;
	setAttr -s 17 ".koy[14:16]"  0.77826881408691406 0 0;
createNode animCurveTA -n "D_:l_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.80964630842208862 1 1;
	setAttr -s 17 ".kiy[14:16]"  -0.58691811561584473 0 0;
	setAttr -s 17 ".kox[14:16]"  0.80964630842208862 1 1;
	setAttr -s 17 ".koy[14:16]"  -0.58691811561584473 0 0;
createNode animCurveTA -n "D_:l_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -4.493922 2 -4.493922 11 -4.493922 15 
		-1.1650893015785846 19 -4.493922 24 -1.6158694721819942 28 -4.493922 32 -3.6382740327396141 
		40 -4.493922 54 -4.493922 63 0 68 -4.493922 75 -4.493922 82 -4.493922 95 -3.3419010434934679 
		101 -3.3419010434934679 108 -3.3419010434934679;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.99806332588195801 1 1;
	setAttr -s 17 ".kiy[14:16]"  -0.062206193804740906 0 0;
	setAttr -s 17 ".kox[14:16]"  0.99806332588195801 1 1;
	setAttr -s 17 ".koy[14:16]"  -0.062206193804740906 0 0;
createNode animCurveTA -n "D_:l_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:l_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:l_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:l_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:l_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:RShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 2.8757973026950432 2 -23.275426785916004 
		6 10.809175549672981 10 -34.08843602875357 14 -28.418812160716907 19 -34.08843602875357 
		23 -33.159761841662778 27 -34.08843602875357 32 -34.08843602875357 40 -34.08843602875357 
		54 -9.2602399738017027 63 -16.410127707309787 72 -22.360744356151425 83 -4.0916631200378442 
		92 -16.281046622854642 98 -8.1378969009838897 108 -8.1378969009838897;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.46811854839324951 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.88366568088531494 0 0;
	setAttr -s 17 ".kox[14:16]"  0.46811854839324951 1 1;
	setAttr -s 17 ".koy[14:16]"  0.88366568088531494 0 0;
createNode animCurveTA -n "D_:RElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 7 0 11 0 15 0 20 0 24 0 28 0 32 
		0 40 0 54 -12.71456298553422 63 -8.8290789010623936 73 -11.122429345226381 84 -12.71456298553422 
		93 0 99 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:R_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 20 ".ktv[0:19]"  0 -69.476750898961853 2 -68.467212072559335 
		6 -31.735107987930775 8 -51.414106878329683 12 -101.7819935241337 14 -82.112970134233265 
		16 -59.709391665101094 21 -101.7819935241337 23 -95.411491630519052 25 -88.104868947082807 
		29 -101.7819935241337 32 -101.7819935241337 40 -101.7819935241337 54 -64.626056039318215 
		63 -33.518107344872654 74 -25.312177883395858 85 -2.9799773189290435 94 0.67516961425184152 
		100 -1.2474624697910552 108 -1.2474624697910552;
	setAttr -s 20 ".kit[11:19]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 20 ".kot[11:19]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 20 ".kix[17:19]"  0.91807252168655396 1 1;
	setAttr -s 20 ".kiy[17:19]"  -0.39641255140304565 0 0;
	setAttr -s 20 ".kox[17:19]"  0.91807252168655396 1 1;
	setAttr -s 20 ".koy[17:19]"  -0.39641255140304565 0 0;
createNode animCurveTA -n "D_:r_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -12.588356 2 -12.588356 9 -12.588356 
		13 -12.588356 17 -12.588356 22 -12.588356 26 -12.588356 30 -12.588356 32 -12.588356 
		40 -12.588356 54 -12.588356 63 -2.709367972602672 75 -2.8640298497852714 82 -12.588356 
		95 -12.498080351386355 101 -12.498080351386355 108 -12.498080351386355;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.77414447069168091 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.63300895690917969 0 0;
	setAttr -s 17 ".kox[14:16]"  0.77414447069168091 1 1;
	setAttr -s 17 ".koy[14:16]"  0.63300895690917969 0 0;
createNode animCurveTA -n "D_:r_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.039968200000000002 2 0.039968200000000002 
		9 0.039968200000000002 13 0.039968200000000002 17 0.039968200000000002 22 0.039968200000000002 
		26 0.039968200000000002 30 0.039968200000000002 32 0.039968200000000002 40 0.039968200000000002 
		54 0.039968200000000002 63 0.039968200000000002 75 0.039968200000000002 82 0.039968200000000002 
		95 4.6514645037620239 101 4.6514645037620239 108 4.6514645037620239;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 5.7762559571042038 75 5.68582497697387 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 -0.48077577619777057 75 -0.47324892383052009 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 0.11943992220111661 75 0.11757001379206609 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 -10.339211663539228 75 -10.177344687522098 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:L_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 -83.218370089439134 2 -85.032427066876707 
		3 -83.218370089439134 7 -54.646973439480767 11 -77.194023432777939 16 -53.013932763478259 
		20 -64.92838865428233 24 -55.226675766118333 32 -55.226675766118333 40 -55.226675766118333 
		54 -24.433459126889961 63 -42.257194571016868 67 -17.826208425377256 69 0 76 0 89 
		0 95 0 108 0;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTA -n "D_:r_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -37.329396 2 -37.329396 9 -37.329396 
		13 -37.329396 17 -37.329396 22 -37.329396 26 -37.329396 30 -37.329396 32 -37.329396 
		40 -37.329396 54 -37.329396 63 -38.463809952303194 75 -38.44604997601347 82 -37.329396 
		95 -67.298744458395092 101 -67.298744458395092 108 -67.298744458395092;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.68329364061355591 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.73014372587203979 0 0;
	setAttr -s 17 ".kox[14:16]"  0.68329364061355591 1 1;
	setAttr -s 17 ".koy[14:16]"  0.73014372587203979 0 0;
createNode animCurveTA -n "D_:r_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -27.828372 2 -27.828372 9 -27.828372 
		13 -27.828372 17 -27.828372 22 -27.828372 26 -27.828372 30 -27.828372 32 -27.828372 
		40 -27.828372 54 -27.828372 63 -4.8287651653811041 75 -5.1888387244975878 82 -27.828372 
		95 -51.590748582374587 101 -51.590748582374587 108 -51.590748582374587;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.44145935773849487 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.8972812294960022 0 0;
	setAttr -s 17 ".kox[14:16]"  0.44145935773849487 1 1;
	setAttr -s 17 ".koy[14:16]"  0.8972812294960022 0 0;
createNode animCurveTA -n "D_:r_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -46.140010000000004 2 -46.140010000000004 
		9 -46.140010000000004 13 -46.140010000000004 17 -46.140010000000004 22 -46.140010000000004 
		26 -46.140010000000004 30 -46.140010000000004 32 -46.140010000000004 40 -46.140010000000004 
		54 -46.140010000000004 63 -36.31928993192183 75 -36.47303958790144 82 -46.140010000000004 
		95 -74.810673852666966 101 -74.810673852666966 108 -74.810673852666966;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.29205828905105591 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.95640051364898682 0 0;
	setAttr -s 17 ".kox[14:16]"  0.29205828905105591 1 1;
	setAttr -s 17 ".koy[14:16]"  0.95640051364898682 0 0;
createNode animCurveTA -n "D_:r_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -16.176127 2 -16.176127 9 -16.176127 
		13 -16.176127 17 -16.176127 22 -16.176127 26 -16.176127 30 -16.176127 32 -16.176127 
		40 -16.176127 54 -16.176127 63 12.450310156856064 75 12.002144967885005 82 -16.176127 
		95 -38.133329475338165 101 -38.133329475338165 108 -38.133329475338165;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.5021672248840332 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.86477053165435791 0 0;
	setAttr -s 17 ".kox[14:16]"  0.5021672248840332 1 1;
	setAttr -s 17 ".koy[14:16]"  0.86477053165435791 0 0;
createNode animCurveTA -n "D_:r_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -0.328974 2 -0.328974 9 -0.328974 13 
		-0.328974 17 -0.328974 22 -0.328974 26 -0.328974 30 -0.328974 32 -0.328974 40 -0.328974 
		54 -0.328974 63 -0.328974 75 -0.328974 82 -0.328974 95 -38.285706776879671 101 -38.285706776879671 
		108 -38.285706776879671;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1.571825 2 -1.571825 9 -1.571825 13 
		-1.571825 17 -1.571825 22 -1.571825 26 -1.571825 30 -1.571825 32 -1.571825 40 -1.571825 
		54 -1.571825 63 6.53619440062909 75 6.40925816947796 82 -1.571825 95 -6.6175135861403929 
		101 -6.6175135861403929 108 -6.6175135861403929;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.99436545372009277 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.10600695759057999 0 0;
	setAttr -s 17 ".kox[14:16]"  0.99436545372009277 1 1;
	setAttr -s 17 ".koy[14:16]"  0.10600695759057999 0 0;
createNode animCurveTA -n "D_:R_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 20 ".ktv[0:19]"  0 6.0150771198115978 2 5.4998800911683263 
		6 2.9099770196098693 8 29.68140688919064 12 22.501381336498007 14 14.764936416258712 
		16 18.621718277308613 21 22.501381336498007 23 25.083122511055439 25 22.95901047719418 
		29 22.501381336498007 32 22.501381336498007 40 22.501381336498007 54 -1.5890925570744994 
		63 26.512306293156495 74 -3.7229768727880619 85 -1.9408293060497566 94 5.1644218226508753 
		100 7.1301771055535337 108 7.1301771055535337;
	setAttr -s 20 ".kit[11:19]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 20 ".kot[11:19]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 20 ".kix[17:19]"  0.62320351600646973 1 1;
	setAttr -s 20 ".kiy[17:19]"  -0.78205966949462891 0 0;
	setAttr -s 20 ".kox[17:19]"  0.62320351600646973 1 1;
	setAttr -s 20 ".koy[17:19]"  -0.78205966949462891 0 0;
createNode animCurveTA -n "D_:RElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 7 0 11 0 15 0 20 0 24 0 28 0 32 
		0 40 0 54 -31.781301041804301 63 -25.635962656066653 73 -29.511704071681063 84 -31.781301041804301 
		93 0 99 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:RShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -9.8468186975466381 2 -9.2490554275731061 
		6 43.831588717232556 10 22.907096454299086 14 17.883256824890438 19 22.907096454299086 
		23 22.084200568243212 27 22.907096454299086 32 22.907096454299086 40 22.907096454299086 
		54 -14.583509843890798 63 -27.133868181567394 72 36.852119298194218 83 27.396813120382077 
		92 48.081621056801815 98 47.399331424850153 108 47.399331424850153;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.90649133920669556 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.42222458124160767 0 0;
	setAttr -s 17 ".kox[14:16]"  0.90649133920669556 1 1;
	setAttr -s 17 ".koy[14:16]"  0.42222458124160767 0 0;
createNode animCurveTA -n "D_:l_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -44.659082 2 -44.659082 11 -44.659082 
		15 -11.578264744363779 19 -44.659082 24 -16.057966120205908 28 -44.659082 32 -36.155940918788197 
		40 -44.659082 54 -44.659082 63 0 68 -44.659082 75 -44.659082 82 -44.659082 95 -58.159773063591629 
		101 -58.159773063591629 108 -58.159773063591629;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.5780453085899353 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.81600469350814819 0 0;
	setAttr -s 17 ".kox[14:16]"  0.5780453085899353 1 1;
	setAttr -s 17 ".koy[14:16]"  0.81600469350814819 0 0;
createNode animCurveTA -n "D_:l_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -38.280067 2 -38.280067 11 -38.280067 
		15 -9.9244482938091565 19 -38.280067 24 -13.764277974369733 28 -38.280067 32 -30.991497744544997 
		40 -38.280067 54 -38.280067 63 0 68 -38.280067 75 -38.280067 82 -38.280067 95 -54.489839590457279 
		101 -54.489839590457279 108 -54.489839590457279;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.76979917287826538 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.63828611373901367 0 0;
	setAttr -s 17 ".kox[14:16]"  0.76979917287826538 1 1;
	setAttr -s 17 ".koy[14:16]"  0.63828611373901367 0 0;
createNode animCurveTA -n "D_:l_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -37.526539 2 -37.526539 11 -37.526539 
		15 -9.7290894488536974 19 -37.526539 24 -13.493333578762538 28 -37.526539 32 -30.381442361499271 
		40 -37.526539 54 -37.526539 63 0 68 -37.526539 75 -37.526539 82 -37.526539 95 -65.326320103044935 
		101 -65.326320103044935 108 -65.326320103044935;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.5033155083656311 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.86410272121429443 0 0;
	setAttr -s 17 ".kox[14:16]"  0.5033155083656311 1 1;
	setAttr -s 17 ".koy[14:16]"  0.86410272121429443 0 0;
createNode animCurveTA -n "D_:l_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -29.654061000000002 2 -29.654061000000002 
		11 -29.654061000000002 15 -7.6880794146980644 19 -29.654061000000002 24 -10.662644306345294 
		28 -29.654061000000002 32 -24.007893360105978 40 -29.654061000000002 54 -29.654061000000002 
		63 0 68 -29.654061000000002 75 -29.654061000000002 82 -29.654061000000002 95 -63.839356559844809 
		101 -63.839356559844809 108 -63.839356559844809;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.45429998636245728 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.89084881544113159 0 0;
	setAttr -s 17 ".kox[14:16]"  0.45429998636245728 1 1;
	setAttr -s 17 ".koy[14:16]"  0.89084881544113159 0 0;
createNode animCurveTA -n "D_:l_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -34.510714 2 -34.510714 11 -34.510714 
		15 -8.9472099585258285 19 -34.510714 24 -12.40894015499657 28 -34.510714 32 -27.939833988145963 
		40 -34.510714 54 -34.510714 63 0 68 -34.510714 75 -34.510714 82 -34.510714 95 -73.966144308812432 
		101 -73.966144308812432 108 -73.966144308812432;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.44346633553504944 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.89629101753234863 0 0;
	setAttr -s 17 ".kox[14:16]"  0.44346633553504944 1 1;
	setAttr -s 17 ".koy[14:16]"  0.89629101753234863 0 0;
createNode animCurveTA -n "D_:l_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -23.031843 2 -23.031843 11 -23.031843 
		15 -5.9712104204162086 19 -23.031843 24 -8.2815082052949069 28 -23.031843 32 -18.646553361794378 
		40 -23.031843 54 -23.031843 63 0 68 -23.031843 75 -23.031843 82 -23.031843 95 -56.359168905796714 
		101 -56.359168905796714 108 -56.359168905796714;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.4584498405456543 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.88872027397155762 0 0;
	setAttr -s 17 ".kox[14:16]"  0.4584498405456543 1 1;
	setAttr -s 17 ".koy[14:16]"  0.88872027397155762 0 0;
createNode animCurveTA -n "D_:l_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 -39.542557426213627 101 -39.542557426213627 108 -39.542557426213627;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.45844170451164246 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.88872450590133667 0 0;
	setAttr -s 17 ".kox[14:16]"  0.45844170451164246 1 1;
	setAttr -s 17 ".koy[14:16]"  0.88872450590133667 0 0;
createNode animCurveTA -n "D_:l_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -7.719601 2 -7.719601 11 -7.719601 15 
		33.650872481541668 19 -7.719601 24 28.048611677232756 28 -7.719601 32 2.9143252970218505 
		40 -7.719601 54 -7.719601 63 48.130511568673342 68 -7.719601 75 -7.719601 82 -7.719601 
		95 -9.399391337933455 101 -9.399391337933455 108 -9.399391337933455;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  0.29237359762191772 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.95630419254302979 0 0;
	setAttr -s 17 ".kox[14:16]"  0.29237359762191772 1 1;
	setAttr -s 17 ".koy[14:16]"  0.95630419254302979 0 0;
createNode animCurveTA -n "D_:L_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -10.931747806621852 2 -10.465062239073518 
		10 -29.337763070546501 14 -28.482457373761967 16 -20.838296677770899 18 -21.70956201003094 
		23 -28.482457373761967 25 -29.626163795737785 27 -28.005974172713991 31 -28.482457373761967 
		32 -28.482457373761967 40 -28.482457373761967 52 29.425942964417654 57 31.377373079800321 
		61 23.442576735293724 63 8.0958788745603076 65 -11.376806283416725 67 -26.629331710266491 
		74 -27.271051456275568 81 -6.1099411514130058 94 -1.1231193682533216 100 -1.1231193682533216 
		108 -1.1231193682533216;
	setAttr -s 23 ".kit[9:22]"  3 3 3 9 3 9 9 9 
		9 9 9 1 3 3;
	setAttr -s 23 ".kot[9:22]"  3 3 3 9 3 9 9 9 
		9 9 9 1 3 3;
	setAttr -s 23 ".kix[20:22]"  0.55166733264923096 1 1;
	setAttr -s 23 ".kiy[20:22]"  0.83406424522399902 0 0;
	setAttr -s 23 ".kox[20:22]"  0.55166733264923096 1 1;
	setAttr -s 23 ".koy[20:22]"  0.83406424522399902 0 0;
createNode animCurveTA -n "D_:LElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 52 3.3267731643559442 57 3.4683156454553417 63 0 73 0 80 0 93 0 99 0 108 0;
	setAttr -s 18 ".kit[8:17]"  3 3 9 3 9 9 9 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 9 3 9 9 9 1 
		3 3;
	setAttr -s 18 ".kix[15:17]"  1 1 1;
	setAttr -s 18 ".kiy[15:17]"  0 0 0;
	setAttr -s 18 ".kox[15:17]"  1 1 1;
	setAttr -s 18 ".koy[15:17]"  0 0 0;
createNode animCurveTA -n "D_:LShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 12.031987391787304 2 13.461731471517512 
		8 -29.585635744269542 12 -26.49961449615429 16 -11.404324795983673 21 -26.49961449615429 
		25 -26.378531162336451 29 -26.49961449615429 32 -26.49961449615429 40 -26.49961449615429 
		57 1.8471136582623546 63 -24.855911707032647 69 -22.111343179568205 72 -24.876075945001507 
		79 -42.760125246827762 92 -45.645013483740549 98 -45.645013483740549 108 -45.645013483740549;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 18 ".kix[15:17]"  0.99974381923675537 1 1;
	setAttr -s 18 ".kiy[15:17]"  0.022634303197264671 0 0;
	setAttr -s 18 ".kox[15:17]"  0.99974387884140015 1 1;
	setAttr -s 18 ".koy[15:17]"  0.022634394466876984 0 0;
createNode animCurveTA -n "D_:TankControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 9 0 13 0 18 0 22 0 26 0 32 
		0 40 0 54 0 63 0 72 0 79 0 92 0 98 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 3 3 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 3 3 3 
		3;
createNode animCurveTA -n "D_:HeadControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 -0.034361300000000067 2 -0.034361300000000108 
		5 -0.034361300000000192 9 -0.034361300000000233 13 -0.034361300000000282 18 -0.034361300000000233 
		22 -0.034361300000000226 26 -0.034361300000000233 32 -0.034361300000000233 40 -0.034361300000000233 
		54 8.2399452265132815 59 6.9049797737067733 63 2.3450581047100432 72 10.384848732608074 
		79 2.6586632542476294 92 2.9071458264266767 101 -2.5139942260181418 108 -0.034361262366484416;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 18 ".kix[15:17]"  0.99999666213989258 1 1;
	setAttr -s 18 ".kiy[15:17]"  -0.0025837605353444815 0 0;
	setAttr -s 18 ".kox[15:17]"  0.99999666213989258 1 1;
	setAttr -s 18 ".koy[15:17]"  -0.0025838264264166355 0 0;
createNode animCurveTA -n "D_:Spine1Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 2.2671858594973879 2 0.99189379387377541 
		4 0 8 0 12 -0.16383960295566991 17 0 21 -0.06773784511184254 25 0 32 0 40 0 59 -3.3795269459645052 
		63 -9.858874321925736 71 1.0343600510565623 78 1.9656299609258852 91 4.8625408286829392 
		100 0.58957667265633396 108 0.019794606311957463;
	setAttr -s 17 ".kit[8:16]"  3 3 9 9 1 9 1 1 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 9 9 1 9 1 1 
		3;
	setAttr -s 17 ".kix[12:16]"  0.99484872817993164 0.99501532316207886 
		0.99998694658279419 0.9965893030166626 1;
	setAttr -s 17 ".kiy[12:16]"  0.10137096792459488 0.099721960723400116 
		0.0051077920943498611 -0.082521654665470123 0;
	setAttr -s 17 ".kox[12:16]"  0.99484872817993164 0.99501532316207886 
		0.99998694658279419 0.9965893030166626 1;
	setAttr -s 17 ".koy[12:16]"  0.10137089341878891 0.099721960723400116 
		0.0051077664829790592 -0.08252166211605072 0;
createNode animCurveTA -n "D_:Spine0Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 3 0 7 0 11 0 16 0 20 0 24 0 32 
		0 40 0 59 -4.7342998477169536 63 -1.6510269797061499 70 -3.8217677611865657 77 0 
		90 0.19532883136751319 98 0.59355684433466127 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 9 9 9 1 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 9 9 9 1 1 3 
		3;
	setAttr -s 17 ".kix[13:16]"  0.99985194206237793 0.99997079372406006 
		1 1;
	setAttr -s 17 ".kiy[13:16]"  0.017207484692335129 0.0076484321616590023 
		0 0;
	setAttr -s 17 ".kox[13:16]"  0.99985194206237793 0.99997079372406006 
		1 1;
	setAttr -s 17 ".koy[13:16]"  0.01720745861530304 0.0076484442688524723 
		0 0;
createNode animCurveTA -n "D_:RootControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 3.4800735764342163 2 3.4800735764342199 
		6 0.93918826059982152 10 3.4800735764342199 15 0.93918826059982152 19 1.8299661886232397 
		23 0.93918826059982152 32 0.93918826059982152 40 0.93918826059982152 59 -23.401330785457361 
		63 -15.018836216377112 69 5.3366921630800928 76 -11.550171573330283 89 0 98 0 108 
		0;
	setAttr -s 16 ".kit[7:15]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 16 ".kot[7:15]"  3 3 9 9 9 9 1 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
	setAttr -s 16 ".kox[13:15]"  1 1 1;
	setAttr -s 16 ".koy[13:15]"  0 0 0;
createNode animCurveTA -n "D_:HipControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 0 2 0 6 0 10 0 15 0 19 0 23 0 32 0 40 
		0 54 2.6042376348959198 63 2.6042376348959198 69 2.6042376348959198 76 2.6042376348959198 
		89 0 95 0 108 0;
	setAttr -s 16 ".kit[7:15]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 16 ".kot[7:15]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 16 ".kix[13:15]"  1 1 1;
	setAttr -s 16 ".kiy[13:15]"  0 0 0;
	setAttr -s 16 ".kox[13:15]"  1 1 1;
	setAttr -s 16 ".koy[13:15]"  0 0 0;
createNode animCurveTA -n "D_:R_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  2 15.22515310007196 5 15.22515310007196 
		9 17.220870324257572 13 24.634013438233971 18 41.11684299443467 22 52.767386001469021 
		26 62.946117168765603 32 62.946117168765603 40 62.946117168765603 51 0 60 0 66 0 
		76 0 83 0.96346260076325818 85 0 89 0 95 0 108 0;
	setAttr -s 18 ".kit[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
	setAttr -s 18 ".kot[7:17]"  3 3 3 9 3 3 9 3 
		3 3 3;
createNode animCurveTA -n "D_:L_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 8.5570733124446932 2 8.1118716945274247 
		3 8.5570733124446932 7 15.56899876299296 11 12.877898365541254 16 14.523315456769369 
		20 16.411256630461022 24 18.504714186249604 32 18.504714186249604 40 18.504714186249604 
		54 22.383881832402853 63 -7.0101069199457688 67 47.981721546159648 69 62.953520182774547 
		76 62.953520182774547 89 62.953520182774547 95 25.46188064100717 108 25.46188064100717;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTA -n "D_:r_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 6.3747289736632835 75 6.2749285117086897 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 -5.3136511338377801 75 -5.2304625253721149 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 -1.0995583780765283 75 -1.0823440881808017 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 9 0 13 0 17 0 22 0 26 0 30 0 32 
		0 40 0 54 0 63 -9.9736977548492085 75 -9.8175531362033563 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.10200200000000001 2 0.10200200000000001 
		9 0.10200200000000001 13 0.10200200000000001 17 0.10200200000000001 22 0.10200200000000001 
		26 0.10200200000000001 30 0.10200200000000001 32 0.10200200000000001 40 0.10200200000000001 
		54 0.10200200000000001 63 0.10200200000000001 75 0.10200200000000001 82 0.10200200000000001 
		95 11.870872562983752 101 11.870872562983752 108 11.870872562983752;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -15.891604000000001 2 -15.891604000000001 
		9 -15.891604000000001 13 -15.891604000000001 17 -15.891604000000001 22 -15.891604000000001 
		26 -15.891604000000001 30 -15.891604000000001 32 -15.891604000000001 40 -15.891604000000001 
		54 -15.891604000000001 63 -39.36194656607389 75 -38.994503330964754 82 -15.891604000000001 
		95 -16.016384857458238 101 -16.016384857458238 108 -16.016384857458238;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.97449719905853271 1 1;
	setAttr -s 17 ".kiy[14:16]"  -0.22439958155155182 0 0;
	setAttr -s 17 ".kox[14:16]"  0.97449719905853271 1 1;
	setAttr -s 17 ".koy[14:16]"  -0.22439958155155182 0 0;
createNode animCurveTA -n "D_:R_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 20 ".ktv[0:19]"  0 -3.1335584812635862 2 -4.5417562588451323 
		6 10.433115202147139 8 19.274478570713924 12 41.928772306339141 14 -8.4211662250189008 
		16 12.25411914057131 21 41.928772306339141 23 22.614744045287512 25 35.405167231556277 
		29 41.928772306339141 32 41.928772306339141 40 41.928772306339141 54 -19.099693880849632 
		63 -7.7137876339276019 74 -23.184573067817251 85 6.6190501187087909 94 0.47456499788026918 
		100 9.6996556887373035 108 9.6996556887373035;
	setAttr -s 20 ".kit[11:19]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 20 ".kot[11:19]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 20 ".kix[17:19]"  0.63277572393417358 1 1;
	setAttr -s 20 ".kiy[17:19]"  0.77433514595031738 0 0;
	setAttr -s 20 ".kox[17:19]"  0.63277572393417358 1 1;
	setAttr -s 20 ".koy[17:19]"  0.77433514595031738 0 0;
createNode animCurveTA -n "D_:RElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 21.067109956317655 2 7.2363470646058152 
		7 39.864956406790128 11 10.997393031614397 15 23.421681574538972 20 10.997393031614397 
		24 16.292957860642307 28 10.997393031614397 32 10.997393031614397 40 10.997393031614397 
		54 50.872563910519688 63 25.190959667762279 73 42.901504613222102 84 50.872563910519688 
		93 36.73362495873517 99 41.523321594528745 108 41.523321594528745;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.51458203792572021 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.85744118690490723 0 0;
	setAttr -s 17 ".kox[14:16]"  0.51458203792572021 1 1;
	setAttr -s 17 ".koy[14:16]"  0.85744118690490723 0 0;
createNode animCurveTA -n "D_:RShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 63.895138519893386 2 74.853045767714477 
		6 56.491487672248724 10 -54.248283482997898 14 -36.127286066840426 19 -54.248283482997898 
		23 -51.280096736031595 27 -54.248283482997898 32 -54.248283482997898 40 -54.248283482997898 
		54 47.823655687685061 63 21.34749480632842 72 -26.212052510829743 83 2.4522004878569885 
		92 -19.976890685692048 98 -12.215538151373286 108 -12.215538151373286;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.44551429152488708 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.89527487754821777 0 0;
	setAttr -s 17 ".kox[14:16]"  0.44551429152488708 1 1;
	setAttr -s 17 ".koy[14:16]"  0.89527487754821777 0 0;
createNode animCurveTA -n "D_:l_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 11 0 15 0 19 0 24 0 28 0 32 0 40 
		0 54 0 63 0 68 0 75 0 82 0 95 0 101 0 108 0;
	setAttr -s 17 ".kit[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kot[7:16]"  3 3 3 9 9 9 9 1 
		3 3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:L_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 -17.589444894367343 2 -16.404027902741348 
		3 -17.589444894367343 7 -36.259762493767042 11 -29.094298879388912 16 -48.825654276848525 
		20 -45.178499673672306 24 -53.467991492174868 32 -53.467991492174868 40 -53.467991492174868 
		54 -23.655379698273251 63 -98.914718323017411 67 -12.151797788864158 69 0 76 0 89 
		0 95 0 108 0;
	setAttr -s 18 ".kit[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
	setAttr -s 18 ".kot[8:17]"  3 3 3 9 9 3 3 3 
		3 3;
createNode animCurveTA -n "D_:r_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -40.679342 2 -40.679342 9 -40.679342 
		13 -40.679342 17 -40.679342 22 -40.679342 26 -40.679342 30 -40.679342 32 -40.679342 
		40 -40.679342 54 -40.679342 63 -40.679342 75 -40.679342 82 -40.679342 95 -56.936986476047892 
		101 -56.936986476047892 108 -56.936986476047892;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  1 1 1;
	setAttr -s 17 ".kiy[14:16]"  0 0 0;
	setAttr -s 17 ".kox[14:16]"  1 1 1;
	setAttr -s 17 ".koy[14:16]"  0 0 0;
createNode animCurveTA -n "D_:r_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -48.302668000000004 2 -48.302668000000004 
		9 -48.302668000000004 13 -48.302668000000004 17 -48.302668000000004 22 -48.302668000000004 
		26 -48.302668000000004 30 -48.302668000000004 32 -48.302668000000004 40 -48.302668000000004 
		54 -48.302668000000004 63 -15.271619667292761 75 -15.788741858250761 82 -48.302668000000004 
		95 -60.240931129090278 101 -60.240931129090278 108 -60.240931129090278;
	setAttr -s 17 ".kit[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kot[8:16]"  3 3 3 9 9 9 1 3 
		3;
	setAttr -s 17 ".kix[14:16]"  0.69016736745834351 1 1;
	setAttr -s 17 ".kiy[14:16]"  0.72364979982376099 0 0;
	setAttr -s 17 ".kox[14:16]"  0.69016736745834351 1 1;
	setAttr -s 17 ".koy[14:16]"  0.72364979982376099 0 0;
select -ne :time1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".o" 42;
select -ne :renderPartition;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 5 ".st";
	setAttr -cb on ".an";
	setAttr -cb on ".pt";
select -ne :renderGlobalsList1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
select -ne :defaultShaderList1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 5 ".s";
select -ne :postProcessList1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 2 ".p";
select -ne :defaultRenderUtilityList1;
	setAttr -s 3 ".u";
select -ne :lightList1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 2 ".ln";
select -ne :defaultTextureList1;
	setAttr -cb on ".cch";
	setAttr -cb on ".ihi";
	setAttr -cb on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 3 ".tx";
select -ne :initialShadingGroup;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -av -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".mwc";
	setAttr -cb on ".an";
	setAttr -cb on ".il";
	setAttr -cb on ".vo";
	setAttr -cb on ".eo";
	setAttr -cb on ".fo";
	setAttr -cb on ".epo";
	setAttr ".ro" yes;
	setAttr -s 8 ".gn";
	setAttr -cb on ".mimt";
	setAttr -cb on ".miop";
	setAttr -cb on ".mise";
	setAttr -cb on ".mism";
	setAttr -cb on ".mice";
	setAttr -av -cb on ".micc";
	setAttr -cb on ".mica";
	setAttr -cb on ".micw";
	setAttr -cb on ".mirw";
select -ne :initialParticleSE;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".mwc";
	setAttr -cb on ".an";
	setAttr -cb on ".il";
	setAttr -cb on ".vo";
	setAttr -cb on ".eo";
	setAttr -cb on ".fo";
	setAttr -cb on ".epo";
	setAttr ".ro" yes;
	setAttr -cb on ".mimt";
	setAttr -cb on ".miop";
	setAttr -cb on ".mise";
	setAttr -cb on ".mism";
	setAttr -cb on ".mice";
	setAttr -cb on ".micc";
	setAttr -cb on ".mica";
	setAttr -cb on ".micw";
	setAttr -cb on ".mirw";
select -ne :defaultRenderGlobals;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".macc";
	setAttr -k on ".macd";
	setAttr -k on ".macq";
	setAttr -k on ".mcfr" 30;
	setAttr -cb on ".ifg";
	setAttr -k on ".clip";
	setAttr -k on ".edm";
	setAttr -k on ".edl";
	setAttr -cb on ".ren";
	setAttr -av -k on ".esr";
	setAttr -k on ".ors";
	setAttr -cb on ".sdf";
	setAttr -k on ".outf";
	setAttr -k on ".gama";
	setAttr ".fs" 0.5;
	setAttr ".ef" 5;
	setAttr -av -k on ".bfs";
	setAttr -k on ".be";
	setAttr -k on ".fec";
	setAttr -k on ".ofc";
	setAttr -cb on ".ofe";
	setAttr -cb on ".efe";
	setAttr -cb on ".umfn";
	setAttr -cb on ".peie";
	setAttr -cb on ".ifp";
	setAttr -k on ".comp";
	setAttr -k on ".cth";
	setAttr -k on ".soll";
	setAttr -k on ".rd";
	setAttr -k on ".lp";
	setAttr -k on ".sp";
	setAttr -k on ".shs";
	setAttr -k on ".lpr";
	setAttr -cb on ".gv";
	setAttr -cb on ".sv";
	setAttr -k on ".mm";
	setAttr -k on ".npu";
	setAttr -k on ".itf";
	setAttr -k on ".shp";
	setAttr -cb on ".isp";
	setAttr -k on ".uf";
	setAttr -k on ".oi";
	setAttr -k on ".rut";
	setAttr -av -k on ".mbf";
	setAttr -k on ".afp";
	setAttr -k on ".pfb";
	setAttr -cb on ".pfrm";
	setAttr -cb on ".pfom";
	setAttr -av -k on ".bll";
	setAttr -k on ".bls";
	setAttr -k on ".smv";
	setAttr -k on ".ubc";
	setAttr -k on ".mbc";
	setAttr -cb on ".mbt";
	setAttr -k on ".udbx";
	setAttr -k on ".smc";
	setAttr -k on ".kmv";
	setAttr -cb on ".isl";
	setAttr -cb on ".ism";
	setAttr -cb on ".imb";
	setAttr -k on ".rlen";
	setAttr -av -k on ".frts";
	setAttr -k on ".tlwd";
	setAttr -k on ".tlht";
	setAttr -k on ".jfc";
	setAttr -cb on ".rsb";
	setAttr -k on ".ope";
	setAttr -k on ".oppf";
	setAttr -cb on ".hbl";
select -ne :defaultLightSet;
	setAttr -k on ".cch";
	setAttr -k on ".nds";
	setAttr -k on ".mwc";
	setAttr ".ro" yes;
select -ne :hardwareRenderGlobals;
	setAttr -k on ".cch";
	setAttr -k on ".nds";
	setAttr ".ctrs" 256;
	setAttr ".btrs" 512;
	setAttr -k off ".eeaa";
	setAttr -k off ".engm";
	setAttr -k off ".mes";
	setAttr -k off ".emb";
	setAttr -k off ".mbbf";
	setAttr -k off ".mbs";
	setAttr -k off ".trm";
	setAttr -k off ".clmt";
	setAttr -k off ".twa";
	setAttr -k off ".twz";
	setAttr -k on ".hwcc";
	setAttr -k on ".hwdp";
	setAttr -k on ".hwql";
	setAttr ".hwfr" 30;
select -ne :defaultHardwareRenderGlobals;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".rp";
	setAttr -k on ".cai";
	setAttr -k on ".coi";
	setAttr -cb on ".bc";
	setAttr -av -k on ".bcb";
	setAttr -av -k on ".bcg";
	setAttr -av -k on ".bcr";
	setAttr -k on ".ei";
	setAttr -k on ".ex";
	setAttr -k on ".es";
	setAttr -av -k on ".ef";
	setAttr -k on ".bf";
	setAttr -k on ".fii";
	setAttr -k on ".sf";
	setAttr -k on ".gr";
	setAttr -k on ".li";
	setAttr -k on ".ls";
	setAttr -k on ".mb";
	setAttr -k on ".ti";
	setAttr -k on ".txt";
	setAttr -k on ".mpr";
	setAttr -k on ".wzd";
	setAttr ".fn" -type "string" "im";
	setAttr -k on ".if";
	setAttr ".res" -type "string" "ntsc_4d 646 485 1.333";
	setAttr -k on ".as";
	setAttr -k on ".ds";
	setAttr -k on ".lm";
	setAttr -k on ".fir";
	setAttr -k on ".aap";
	setAttr -k on ".gh";
	setAttr -cb on ".sd";
select -ne :ikSystem;
	setAttr -av ".gsn";
	setAttr -s 2 ".sol";
connectAttr "D_:Entity_translateX.o" "D_RN.phl[2318]";
connectAttr "D_:Entity_translateY.o" "D_RN.phl[2319]";
connectAttr "D_:Entity_translateZ.o" "D_RN.phl[2320]";
connectAttr "D_:Entity_rotateX.o" "D_RN.phl[2321]";
connectAttr "D_:Entity_rotateY.o" "D_RN.phl[2322]";
connectAttr "D_:Entity_rotateZ.o" "D_RN.phl[2323]";
connectAttr "D_:Entity_visibility.o" "D_RN.phl[2324]";
connectAttr "D_:Entity_scaleX.o" "D_RN.phl[2325]";
connectAttr "D_:Entity_scaleY.o" "D_RN.phl[2326]";
connectAttr "D_:Entity_scaleZ.o" "D_RN.phl[2327]";
connectAttr "D_:DiverGlobal_translateX.o" "D_RN.phl[2328]";
connectAttr "D_:DiverGlobal_translateY.o" "D_RN.phl[2329]";
connectAttr "D_:DiverGlobal_translateZ.o" "D_RN.phl[2330]";
connectAttr "D_:DiverGlobal_rotateX.o" "D_RN.phl[2331]";
connectAttr "D_:DiverGlobal_rotateY.o" "D_RN.phl[2332]";
connectAttr "D_:DiverGlobal_rotateZ.o" "D_RN.phl[2333]";
connectAttr "D_:DiverGlobal_scaleX.o" "D_RN.phl[2334]";
connectAttr "D_:DiverGlobal_scaleY.o" "D_RN.phl[2335]";
connectAttr "D_:DiverGlobal_scaleZ.o" "D_RN.phl[2336]";
connectAttr "D_:DiverGlobal_visibility.o" "D_RN.phl[2337]";
connectAttr "D_:l_mid_1_rotateX.o" "D_RN.phl[2338]";
connectAttr "D_:l_mid_1_rotateY.o" "D_RN.phl[2339]";
connectAttr "D_:l_mid_1_rotateZ.o" "D_RN.phl[2340]";
connectAttr "D_:l_mid_1_visibility.o" "D_RN.phl[2341]";
connectAttr "D_:l_mid_2_rotateX.o" "D_RN.phl[2342]";
connectAttr "D_:l_mid_2_rotateY.o" "D_RN.phl[2343]";
connectAttr "D_:l_mid_2_rotateZ.o" "D_RN.phl[2344]";
connectAttr "D_:l_mid_2_visibility.o" "D_RN.phl[2345]";
connectAttr "D_:l_pink_1_rotateX.o" "D_RN.phl[2346]";
connectAttr "D_:l_pink_1_rotateY.o" "D_RN.phl[2347]";
connectAttr "D_:l_pink_1_rotateZ.o" "D_RN.phl[2348]";
connectAttr "D_:l_pink_1_visibility.o" "D_RN.phl[2349]";
connectAttr "D_:l_pink_2_rotateX.o" "D_RN.phl[2350]";
connectAttr "D_:l_pink_2_rotateY.o" "D_RN.phl[2351]";
connectAttr "D_:l_pink_2_rotateZ.o" "D_RN.phl[2352]";
connectAttr "D_:l_pink_2_visibility.o" "D_RN.phl[2353]";
connectAttr "D_:l_point_1_rotateX.o" "D_RN.phl[2354]";
connectAttr "D_:l_point_1_rotateY.o" "D_RN.phl[2355]";
connectAttr "D_:l_point_1_rotateZ.o" "D_RN.phl[2356]";
connectAttr "D_:l_point_1_visibility.o" "D_RN.phl[2357]";
connectAttr "D_:l_point_2_rotateX.o" "D_RN.phl[2358]";
connectAttr "D_:l_point_2_rotateY.o" "D_RN.phl[2359]";
connectAttr "D_:l_point_2_rotateZ.o" "D_RN.phl[2360]";
connectAttr "D_:l_point_2_visibility.o" "D_RN.phl[2361]";
connectAttr "D_:l_thumb_1_rotateX.o" "D_RN.phl[2362]";
connectAttr "D_:l_thumb_1_rotateY.o" "D_RN.phl[2363]";
connectAttr "D_:l_thumb_1_rotateZ.o" "D_RN.phl[2364]";
connectAttr "D_:l_thumb_1_visibility.o" "D_RN.phl[2365]";
connectAttr "D_:l_thumb_2_rotateX.o" "D_RN.phl[2366]";
connectAttr "D_:l_thumb_2_rotateY.o" "D_RN.phl[2367]";
connectAttr "D_:l_thumb_2_rotateZ.o" "D_RN.phl[2368]";
connectAttr "D_:l_thumb_2_visibility.o" "D_RN.phl[2369]";
connectAttr "D_:r_mid_1_rotateX.o" "D_RN.phl[2370]";
connectAttr "D_:r_mid_1_rotateY.o" "D_RN.phl[2371]";
connectAttr "D_:r_mid_1_rotateZ.o" "D_RN.phl[2372]";
connectAttr "D_:r_mid_1_visibility.o" "D_RN.phl[2373]";
connectAttr "D_:r_mid_2_rotateX.o" "D_RN.phl[2374]";
connectAttr "D_:r_mid_2_rotateY.o" "D_RN.phl[2375]";
connectAttr "D_:r_mid_2_rotateZ.o" "D_RN.phl[2376]";
connectAttr "D_:r_mid_2_visibility.o" "D_RN.phl[2377]";
connectAttr "D_:r_pink_1_rotateX.o" "D_RN.phl[2378]";
connectAttr "D_:r_pink_1_rotateY.o" "D_RN.phl[2379]";
connectAttr "D_:r_pink_1_rotateZ.o" "D_RN.phl[2380]";
connectAttr "D_:r_pink_1_visibility.o" "D_RN.phl[2381]";
connectAttr "D_:r_pink_2_rotateX.o" "D_RN.phl[2382]";
connectAttr "D_:r_pink_2_rotateY.o" "D_RN.phl[2383]";
connectAttr "D_:r_pink_2_rotateZ.o" "D_RN.phl[2384]";
connectAttr "D_:r_pink_2_visibility.o" "D_RN.phl[2385]";
connectAttr "D_:r_point_1_rotateX.o" "D_RN.phl[2386]";
connectAttr "D_:r_point_1_rotateY.o" "D_RN.phl[2387]";
connectAttr "D_:r_point_1_rotateZ.o" "D_RN.phl[2388]";
connectAttr "D_:r_point_1_visibility.o" "D_RN.phl[2389]";
connectAttr "D_:r_point_2_rotateX.o" "D_RN.phl[2390]";
connectAttr "D_:r_point_2_rotateY.o" "D_RN.phl[2391]";
connectAttr "D_:r_point_2_rotateZ.o" "D_RN.phl[2392]";
connectAttr "D_:r_point_2_visibility.o" "D_RN.phl[2393]";
connectAttr "D_:r_thumb_1_rotateX.o" "D_RN.phl[2394]";
connectAttr "D_:r_thumb_1_rotateY.o" "D_RN.phl[2395]";
connectAttr "D_:r_thumb_1_rotateZ.o" "D_RN.phl[2396]";
connectAttr "D_:r_thumb_1_visibility.o" "D_RN.phl[2397]";
connectAttr "D_:r_thumb_2_rotateX.o" "D_RN.phl[2398]";
connectAttr "D_:r_thumb_2_rotateY.o" "D_RN.phl[2399]";
connectAttr "D_:r_thumb_2_rotateZ.o" "D_RN.phl[2400]";
connectAttr "D_:r_thumb_2_visibility.o" "D_RN.phl[2401]";
connectAttr "D_:L_Foot_ToeRoll.o" "D_RN.phl[2402]";
connectAttr "D_:L_Foot_BallRoll.o" "D_RN.phl[2403]";
connectAttr "D_:L_Foot_translateX.o" "D_RN.phl[2404]";
connectAttr "D_:L_Foot_translateY.o" "D_RN.phl[2405]";
connectAttr "D_:L_Foot_translateZ.o" "D_RN.phl[2406]";
connectAttr "D_:L_Foot_rotateX.o" "D_RN.phl[2407]";
connectAttr "D_:L_Foot_rotateY.o" "D_RN.phl[2408]";
connectAttr "D_:L_Foot_rotateZ.o" "D_RN.phl[2409]";
connectAttr "D_:L_Foot_scaleX.o" "D_RN.phl[2410]";
connectAttr "D_:L_Foot_scaleY.o" "D_RN.phl[2411]";
connectAttr "D_:L_Foot_scaleZ.o" "D_RN.phl[2412]";
connectAttr "D_:L_Foot_visibility.o" "D_RN.phl[2413]";
connectAttr "D_:R_Foot_ToeRoll.o" "D_RN.phl[2414]";
connectAttr "D_:R_Foot_BallRoll.o" "D_RN.phl[2415]";
connectAttr "D_:R_Foot_translateX.o" "D_RN.phl[2416]";
connectAttr "D_:R_Foot_translateY.o" "D_RN.phl[2417]";
connectAttr "D_:R_Foot_translateZ.o" "D_RN.phl[2418]";
connectAttr "D_:R_Foot_rotateX.o" "D_RN.phl[2419]";
connectAttr "D_:R_Foot_rotateY.o" "D_RN.phl[2420]";
connectAttr "D_:R_Foot_rotateZ.o" "D_RN.phl[2421]";
connectAttr "D_:R_Foot_scaleX.o" "D_RN.phl[2422]";
connectAttr "D_:R_Foot_scaleY.o" "D_RN.phl[2423]";
connectAttr "D_:R_Foot_scaleZ.o" "D_RN.phl[2424]";
connectAttr "D_:R_Foot_visibility.o" "D_RN.phl[2425]";
connectAttr "D_:L_Knee_translateX.o" "D_RN.phl[2426]";
connectAttr "D_:L_Knee_translateY.o" "D_RN.phl[2427]";
connectAttr "D_:L_Knee_translateZ.o" "D_RN.phl[2428]";
connectAttr "D_:L_Knee_scaleX.o" "D_RN.phl[2429]";
connectAttr "D_:L_Knee_scaleY.o" "D_RN.phl[2430]";
connectAttr "D_:L_Knee_scaleZ.o" "D_RN.phl[2431]";
connectAttr "D_:L_Knee_visibility.o" "D_RN.phl[2432]";
connectAttr "D_:R_Knee_translateX.o" "D_RN.phl[2433]";
connectAttr "D_:R_Knee_translateY.o" "D_RN.phl[2434]";
connectAttr "D_:R_Knee_translateZ.o" "D_RN.phl[2435]";
connectAttr "D_:R_Knee_scaleX.o" "D_RN.phl[2436]";
connectAttr "D_:R_Knee_scaleY.o" "D_RN.phl[2437]";
connectAttr "D_:R_Knee_scaleZ.o" "D_RN.phl[2438]";
connectAttr "D_:R_Knee_visibility.o" "D_RN.phl[2439]";
connectAttr "D_:R_KneeShape_localPositionX.o" "D_RN.phl[2440]";
connectAttr "D_:R_KneeShape_localPositionY.o" "D_RN.phl[2441]";
connectAttr "D_:R_KneeShape_localPositionZ.o" "D_RN.phl[2442]";
connectAttr "D_:R_KneeShape_localScaleX.o" "D_RN.phl[2443]";
connectAttr "D_:R_KneeShape_localScaleY.o" "D_RN.phl[2444]";
connectAttr "D_:R_KneeShape_localScaleZ.o" "D_RN.phl[2445]";
connectAttr "D_:RootControl_translateX.o" "D_RN.phl[2446]";
connectAttr "D_:RootControl_translateY.o" "D_RN.phl[2447]";
connectAttr "D_:RootControl_translateZ1.o" "D_RN.phl[2448]";
connectAttr "D_:RootControl_rotateX.o" "D_RN.phl[2449]";
connectAttr "D_:RootControl_rotateY.o" "D_RN.phl[2450]";
connectAttr "D_:RootControl_rotateZ.o" "D_RN.phl[2451]";
connectAttr "D_:RootControl_scaleX.o" "D_RN.phl[2452]";
connectAttr "D_:RootControl_scaleY.o" "D_RN.phl[2453]";
connectAttr "D_:RootControl_scaleZ.o" "D_RN.phl[2454]";
connectAttr "D_:RootControl_visibility.o" "D_RN.phl[2455]";
connectAttr "D_:Spine0Control_translateX.o" "D_RN.phl[2456]";
connectAttr "D_:Spine0Control_translateY.o" "D_RN.phl[2457]";
connectAttr "D_:Spine0Control_translateZ.o" "D_RN.phl[2458]";
connectAttr "D_:Spine0Control_scaleX.o" "D_RN.phl[2459]";
connectAttr "D_:Spine0Control_scaleY.o" "D_RN.phl[2460]";
connectAttr "D_:Spine0Control_scaleZ.o" "D_RN.phl[2461]";
connectAttr "D_:Spine0Control_rotateX.o" "D_RN.phl[2462]";
connectAttr "D_:Spine0Control_rotateY.o" "D_RN.phl[2463]";
connectAttr "D_:Spine0Control_rotateZ.o" "D_RN.phl[2464]";
connectAttr "D_:Spine0Control_visibility.o" "D_RN.phl[2465]";
connectAttr "D_:Spine1Control_scaleX.o" "D_RN.phl[2466]";
connectAttr "D_:Spine1Control_scaleY.o" "D_RN.phl[2467]";
connectAttr "D_:Spine1Control_scaleZ.o" "D_RN.phl[2468]";
connectAttr "D_:Spine1Control_rotateX.o" "D_RN.phl[2469]";
connectAttr "D_:Spine1Control_rotateY.o" "D_RN.phl[2470]";
connectAttr "D_:Spine1Control_rotateZ.o" "D_RN.phl[2471]";
connectAttr "D_:Spine1Control_visibility.o" "D_RN.phl[2472]";
connectAttr "D_:TankControl_scaleX.o" "D_RN.phl[2473]";
connectAttr "D_:TankControl_scaleY.o" "D_RN.phl[2474]";
connectAttr "D_:TankControl_scaleZ.o" "D_RN.phl[2475]";
connectAttr "D_:TankControl_rotateX.o" "D_RN.phl[2476]";
connectAttr "D_:TankControl_rotateY.o" "D_RN.phl[2477]";
connectAttr "D_:TankControl_rotateZ.o" "D_RN.phl[2478]";
connectAttr "D_:TankControl_translateX.o" "D_RN.phl[2479]";
connectAttr "D_:TankControl_translateY.o" "D_RN.phl[2480]";
connectAttr "D_:TankControl_translateZ.o" "D_RN.phl[2481]";
connectAttr "D_:TankControl_visibility.o" "D_RN.phl[2482]";
connectAttr "D_:L_Clavicle_scaleX.o" "D_RN.phl[2483]";
connectAttr "D_:L_Clavicle_scaleY.o" "D_RN.phl[2484]";
connectAttr "D_:L_Clavicle_scaleZ.o" "D_RN.phl[2485]";
connectAttr "D_:L_Clavicle_translateX.o" "D_RN.phl[2486]";
connectAttr "D_:L_Clavicle_translateY.o" "D_RN.phl[2487]";
connectAttr "D_:L_Clavicle_translateZ.o" "D_RN.phl[2488]";
connectAttr "D_:L_Clavicle_visibility.o" "D_RN.phl[2489]";
connectAttr "D_:R_Clavicle_scaleX.o" "D_RN.phl[2490]";
connectAttr "D_:R_Clavicle_scaleY.o" "D_RN.phl[2491]";
connectAttr "D_:R_Clavicle_scaleZ.o" "D_RN.phl[2492]";
connectAttr "D_:R_Clavicle_translateX.o" "D_RN.phl[2493]";
connectAttr "D_:R_Clavicle_translateY.o" "D_RN.phl[2494]";
connectAttr "D_:R_Clavicle_translateZ.o" "D_RN.phl[2495]";
connectAttr "D_:R_Clavicle_visibility.o" "D_RN.phl[2496]";
connectAttr "D_:HeadControl_translateX.o" "D_RN.phl[2497]";
connectAttr "D_:HeadControl_translateY.o" "D_RN.phl[2498]";
connectAttr "D_:HeadControl_translateZ.o" "D_RN.phl[2499]";
connectAttr "D_:HeadControl_scaleX.o" "D_RN.phl[2500]";
connectAttr "D_:HeadControl_scaleY.o" "D_RN.phl[2501]";
connectAttr "D_:HeadControl_scaleZ.o" "D_RN.phl[2502]";
connectAttr "D_:HeadControl_Mask.o" "D_RN.phl[2503]";
connectAttr "D_:HeadControl_rotateX.o" "D_RN.phl[2504]";
connectAttr "D_:HeadControl_rotateY.o" "D_RN.phl[2505]";
connectAttr "D_:HeadControl_rotateZ.o" "D_RN.phl[2506]";
connectAttr "D_:HeadControl_visibility.o" "D_RN.phl[2507]";
connectAttr "D_:LShoulderFK_translateX.o" "D_RN.phl[2508]";
connectAttr "D_:LShoulderFK_translateY.o" "D_RN.phl[2509]";
connectAttr "D_:LShoulderFK_translateZ.o" "D_RN.phl[2510]";
connectAttr "D_:LShoulderFK_scaleX.o" "D_RN.phl[2511]";
connectAttr "D_:LShoulderFK_scaleY.o" "D_RN.phl[2512]";
connectAttr "D_:LShoulderFK_scaleZ.o" "D_RN.phl[2513]";
connectAttr "D_:LShoulderFK_rotateX.o" "D_RN.phl[2514]";
connectAttr "D_:LShoulderFK_rotateY.o" "D_RN.phl[2515]";
connectAttr "D_:LShoulderFK_rotateZ.o" "D_RN.phl[2516]";
connectAttr "D_:LShoulderFK_visibility.o" "D_RN.phl[2517]";
connectAttr "D_:LElbowFK_scaleX.o" "D_RN.phl[2518]";
connectAttr "D_:LElbowFK_scaleY.o" "D_RN.phl[2519]";
connectAttr "D_:LElbowFK_scaleZ.o" "D_RN.phl[2520]";
connectAttr "D_:LElbowFK_rotateX.o" "D_RN.phl[2521]";
connectAttr "D_:LElbowFK_rotateY.o" "D_RN.phl[2522]";
connectAttr "D_:LElbowFK_rotateZ.o" "D_RN.phl[2523]";
connectAttr "D_:LElbowFK_visibility.o" "D_RN.phl[2524]";
connectAttr "D_:L_Wrist_scaleX.o" "D_RN.phl[2525]";
connectAttr "D_:L_Wrist_scaleY.o" "D_RN.phl[2526]";
connectAttr "D_:L_Wrist_scaleZ.o" "D_RN.phl[2527]";
connectAttr "D_:L_Wrist_rotateX.o" "D_RN.phl[2528]";
connectAttr "D_:L_Wrist_rotateY.o" "D_RN.phl[2529]";
connectAttr "D_:L_Wrist_rotateZ.o" "D_RN.phl[2530]";
connectAttr "D_:L_Wrist_visibility.o" "D_RN.phl[2531]";
connectAttr "D_:RShoulderFK_scaleX.o" "D_RN.phl[2532]";
connectAttr "D_:RShoulderFK_scaleY.o" "D_RN.phl[2533]";
connectAttr "D_:RShoulderFK_scaleZ.o" "D_RN.phl[2534]";
connectAttr "D_:RShoulderFK_rotateX.o" "D_RN.phl[2535]";
connectAttr "D_:RShoulderFK_rotateY.o" "D_RN.phl[2536]";
connectAttr "D_:RShoulderFK_rotateZ.o" "D_RN.phl[2537]";
connectAttr "D_:RShoulderFK_visibility.o" "D_RN.phl[2538]";
connectAttr "D_:RElbowFK_scaleX1.o" "D_RN.phl[2539]";
connectAttr "D_:RElbowFK_scaleY1.o" "D_RN.phl[2540]";
connectAttr "D_:RElbowFK_scaleZ1.o" "D_RN.phl[2541]";
connectAttr "D_:RElbowFK_rotateX.o" "D_RN.phl[2542]";
connectAttr "D_:RElbowFK_rotateY.o" "D_RN.phl[2543]";
connectAttr "D_:RElbowFK_rotateZ.o" "D_RN.phl[2544]";
connectAttr "D_:RElbowFK_visibility.o" "D_RN.phl[2545]";
connectAttr "D_:R_Wrist_scaleX.o" "D_RN.phl[2546]";
connectAttr "D_:R_Wrist_scaleY.o" "D_RN.phl[2547]";
connectAttr "D_:R_Wrist_scaleZ.o" "D_RN.phl[2548]";
connectAttr "D_:R_Wrist_rotateX.o" "D_RN.phl[2549]";
connectAttr "D_:R_Wrist_rotateY.o" "D_RN.phl[2550]";
connectAttr "D_:R_Wrist_rotateZ.o" "D_RN.phl[2551]";
connectAttr "D_:R_Wrist_visibility.o" "D_RN.phl[2552]";
connectAttr "D_:HipControl_scaleX.o" "D_RN.phl[2553]";
connectAttr "D_:HipControl_scaleY.o" "D_RN.phl[2554]";
connectAttr "D_:HipControl_scaleZ.o" "D_RN.phl[2555]";
connectAttr "D_:HipControl_rotateX.o" "D_RN.phl[2556]";
connectAttr "D_:HipControl_rotateY.o" "D_RN.phl[2557]";
connectAttr "D_:HipControl_rotateZ.o" "D_RN.phl[2558]";
connectAttr "D_:HipControl_visibility.o" "D_RN.phl[2559]";
connectAttr ":defaultLightSet.msg" "lightLinker1.lnk[0].llnk";
connectAttr ":initialShadingGroup.msg" "lightLinker1.lnk[0].olnk";
connectAttr ":defaultLightSet.msg" "lightLinker1.lnk[1].llnk";
connectAttr ":initialParticleSE.msg" "lightLinker1.lnk[1].olnk";
connectAttr ":defaultLightSet.msg" "lightLinker1.slnk[0].sllk";
connectAttr ":initialShadingGroup.msg" "lightLinker1.slnk[0].solk";
connectAttr ":defaultLightSet.msg" "lightLinker1.slnk[1].sllk";
connectAttr ":initialParticleSE.msg" "lightLinker1.slnk[1].solk";
connectAttr "layerManager.dli[0]" "defaultLayer.id";
connectAttr "renderLayerManager.rlmi[0]" "defaultRenderLayer.rlid";
connectAttr "D_:RElbowFK_scaleX.o" "D_RN.phl[2315]";
connectAttr "D_:RElbowFK_scaleY.o" "D_RN.phl[2316]";
connectAttr "D_:RElbowFK_scaleZ.o" "D_RN.phl[2317]";
connectAttr "lightLinker1.msg" ":lightList1.ln" -na;
// End of diver_getup.ma
