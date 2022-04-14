//Maya ASCII 2008 scene
//Name: diver_hitreact2.ma
//Last modified: Mon, Aug 25, 2008 12:08:27 PM
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
	setAttr ".t" -type "double3" -184.48828933304438 75.596111990861814 244.52270414788697 ;
	setAttr ".r" -type "double3" -11.738367604497155 -32.600000000002701 4.7191905037641983e-016 ;
createNode camera -s -n "perspShape" -p "persp";
	setAttr -k off ".v" no;
	setAttr ".fl" 34.999999999999986;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 310.32307118793597;
	setAttr ".imn" -type "string" "persp";
	setAttr ".den" -type "string" "persp_depth";
	setAttr ".man" -type "string" "persp_mask";
	setAttr ".tp" -type "double3" -19.351449127515505 76.671425889706697 -44.12818595500265 ;
	setAttr ".hc" -type "string" "viewSet -p %camera";
createNode transform -s -n "top";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 5000.1000000000004 1.0981356933150269e-012 ;
	setAttr ".r" -type "double3" -89.999999999999986 0 0 ;
createNode camera -s -n "topShape" -p "top";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 359.62025676471194;
	setAttr ".imn" -type "string" "top";
	setAttr ".den" -type "string" "top_depth";
	setAttr ".man" -type "string" "top_mask";
	setAttr ".hc" -type "string" "viewSet -t %camera";
	setAttr ".o" yes;
createNode transform -s -n "front";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 54.298470621324682 5000.3948178030723 ;
createNode camera -s -n "frontShape" -p "front";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 259.04115415593367;
	setAttr ".imn" -type "string" "front";
	setAttr ".den" -type "string" "front_depth";
	setAttr ".man" -type "string" "front_mask";
	setAttr ".hc" -type "string" "viewSet -f %camera";
	setAttr ".o" yes;
createNode transform -s -n "side";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 5000.3829985830753 53.198895840651076 1.1103080673942223e-012 ;
	setAttr ".r" -type "double3" 0 89.999999999999986 0 ;
createNode camera -s -n "sideShape" -p "side";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 319.55860543030292;
	setAttr ".imn" -type "string" "side";
	setAttr ".den" -type "string" "side_depth";
	setAttr ".man" -type "string" "side_mask";
	setAttr ".hc" -type "string" "viewSet -s %camera";
	setAttr ".o" yes;
createNode transform -n "pPlane1";
	setAttr ".s" -type "double3" 811.2231927876793 811.2231927876793 811.2231927876793 ;
createNode mesh -n "pPlaneShape1" -p "pPlane1";
	setAttr -k off ".v";
	setAttr ".vir" yes;
	setAttr ".vif" yes;
	setAttr ".uvst[0].uvsn" -type "string" "map1";
	setAttr ".cuvs" -type "string" "map1";
	setAttr ".dcc" -type "string" "Ambient+Diffuse";
	setAttr ".covm[0]"  0 1 1;
	setAttr ".cdvm[0]"  0 1 1;
createNode colladaDocument -n "colladaDocuments";
	setAttr ".doc[0].fn" -type "string" "";
createNode colladaDocument -n "colladaDocuments1";
	setAttr ".doc[0].fn" -type "string" "";
createNode colladaDocument -n "colladaDocuments2";
createNode lightLinker -n "lightLinker1";
	setAttr -s 2 ".lnk";
	setAttr -s 2 ".slnk";
createNode displayLayerManager -n "layerManager";
	setAttr ".cdl" 1;
	setAttr -s 2 ".dli[1]"  1;
	setAttr -s 2 ".dli";
createNode displayLayer -n "defaultLayer";
createNode renderLayerManager -n "renderLayerManager";
createNode renderLayer -n "defaultRenderLayer";
	setAttr ".g" yes;
createNode reference -n "D_RN";
	setAttr -s 440 ".phl";
	setAttr ".phl[1964]" 0;
	setAttr ".phl[2015]" 0;
	setAttr ".phl[2074]" 0;
	setAttr ".phl[2125]" 0;
	setAttr ".phl[3466]" 0;
	setAttr ".phl[3467]" 0;
	setAttr ".phl[3468]" 0;
	setAttr ".phl[3469]" 0;
	setAttr ".phl[3470]" 0;
	setAttr ".phl[3471]" 0;
	setAttr ".phl[3472]" 0;
	setAttr ".phl[3473]" 0;
	setAttr ".phl[3474]" 0;
	setAttr ".phl[3475]" 0;
	setAttr ".phl[3476]" 0;
	setAttr ".phl[3477]" 0;
	setAttr ".phl[3478]" 0;
	setAttr ".phl[3479]" 0;
	setAttr ".phl[3480]" 0;
	setAttr ".phl[3481]" 0;
	setAttr ".phl[3482]" 0;
	setAttr ".phl[3483]" 0;
	setAttr ".phl[3484]" 0;
	setAttr ".phl[3485]" 0;
	setAttr ".phl[3486]" 0;
	setAttr ".phl[3487]" 0;
	setAttr ".phl[3488]" 0;
	setAttr ".phl[3489]" 0;
	setAttr ".phl[3490]" 0;
	setAttr ".phl[3491]" 0;
	setAttr ".phl[3492]" 0;
	setAttr ".phl[3493]" 0;
	setAttr ".phl[3494]" 0;
	setAttr ".phl[3495]" 0;
	setAttr ".phl[3496]" 0;
	setAttr ".phl[3497]" 0;
	setAttr ".phl[3498]" 0;
	setAttr ".phl[3499]" 0;
	setAttr ".phl[3500]" 0;
	setAttr ".phl[3501]" 0;
	setAttr ".phl[3502]" 0;
	setAttr ".phl[3503]" 0;
	setAttr ".phl[3504]" 0;
	setAttr ".phl[3505]" 0;
	setAttr ".phl[3506]" 0;
	setAttr ".phl[3507]" 0;
	setAttr ".phl[3508]" 0;
	setAttr ".phl[3509]" 0;
	setAttr ".phl[3510]" 0;
	setAttr ".phl[3511]" 0;
	setAttr ".phl[3512]" 0;
	setAttr ".phl[3513]" 0;
	setAttr ".phl[3514]" 0;
	setAttr ".phl[3515]" 0;
	setAttr ".phl[3516]" 0;
	setAttr ".phl[3517]" 0;
	setAttr ".phl[3518]" 0;
	setAttr ".phl[3519]" 0;
	setAttr ".phl[3520]" 0;
	setAttr ".phl[3521]" 0;
	setAttr ".phl[3522]" 0;
	setAttr ".phl[3523]" 0;
	setAttr ".phl[3524]" 0;
	setAttr ".phl[3525]" 0;
	setAttr ".phl[3526]" 0;
	setAttr ".phl[3527]" 0;
	setAttr ".phl[3528]" 0;
	setAttr ".phl[3529]" 0;
	setAttr ".phl[3530]" 0;
	setAttr ".phl[3531]" 0;
	setAttr ".phl[3532]" 0;
	setAttr ".phl[3533]" 0;
	setAttr ".phl[3534]" 0;
	setAttr ".phl[3535]" 0;
	setAttr ".phl[3536]" 0;
	setAttr ".phl[3537]" 0;
	setAttr ".phl[3538]" 0;
	setAttr ".phl[3539]" 0;
	setAttr ".phl[3540]" 0;
	setAttr ".phl[3541]" 0;
	setAttr ".phl[3542]" 0;
	setAttr ".phl[3543]" 0;
	setAttr ".phl[3544]" 0;
	setAttr ".phl[3545]" 0;
	setAttr ".phl[3546]" 0;
	setAttr ".phl[3547]" 0;
	setAttr ".phl[3548]" 0;
	setAttr ".phl[3549]" 0;
	setAttr ".phl[3550]" 0;
	setAttr ".phl[3551]" 0;
	setAttr ".phl[3552]" 0;
	setAttr ".phl[3553]" 0;
	setAttr ".phl[3554]" 0;
	setAttr ".phl[3555]" 0;
	setAttr ".phl[3556]" 0;
	setAttr ".phl[3557]" 0;
	setAttr ".phl[3558]" 0;
	setAttr ".phl[3559]" 0;
	setAttr ".phl[3560]" 0;
	setAttr ".phl[3561]" 0;
	setAttr ".phl[3562]" 0;
	setAttr ".phl[3563]" 0;
	setAttr ".phl[3564]" 0;
	setAttr ".phl[3565]" 0;
	setAttr ".phl[3566]" 0;
	setAttr ".phl[3567]" 0;
	setAttr ".phl[3568]" 0;
	setAttr ".phl[3569]" 0;
	setAttr ".phl[3570]" 0;
	setAttr ".phl[3571]" 0;
	setAttr ".phl[3572]" 0;
	setAttr ".phl[3573]" 0;
	setAttr ".phl[3574]" 0;
	setAttr ".phl[3575]" 0;
	setAttr ".phl[3576]" 0;
	setAttr ".phl[3577]" 0;
	setAttr ".phl[3578]" 0;
	setAttr ".phl[3579]" 0;
	setAttr ".phl[3580]" 0;
	setAttr ".phl[3581]" 0;
	setAttr ".phl[3582]" 0;
	setAttr ".phl[3583]" 0;
	setAttr ".phl[3584]" 0;
	setAttr ".phl[3585]" 0;
	setAttr ".phl[3586]" 0;
	setAttr ".phl[3587]" 0;
	setAttr ".phl[3588]" 0;
	setAttr ".phl[3589]" 0;
	setAttr ".phl[3590]" 0;
	setAttr ".phl[3591]" 0;
	setAttr ".phl[3592]" 0;
	setAttr ".phl[3593]" 0;
	setAttr ".phl[3594]" 0;
	setAttr ".phl[3595]" 0;
	setAttr ".phl[3596]" 0;
	setAttr ".phl[3597]" 0;
	setAttr ".phl[3598]" 0;
	setAttr ".phl[3599]" 0;
	setAttr ".phl[3600]" 0;
	setAttr ".phl[3601]" 0;
	setAttr ".phl[3602]" 0;
	setAttr ".phl[3603]" 0;
	setAttr ".phl[3604]" 0;
	setAttr ".phl[3605]" 0;
	setAttr ".phl[3606]" 0;
	setAttr ".phl[3607]" 0;
	setAttr ".phl[3608]" 0;
	setAttr ".phl[3609]" 0;
	setAttr ".phl[3610]" 0;
	setAttr ".phl[3611]" 0;
	setAttr ".phl[3612]" 0;
	setAttr ".phl[3613]" 0;
	setAttr ".phl[3614]" 0;
	setAttr ".phl[3615]" 0;
	setAttr ".phl[3616]" 0;
	setAttr ".phl[3617]" 0;
	setAttr ".phl[3618]" 0;
	setAttr ".phl[3619]" 0;
	setAttr ".phl[3620]" 0;
	setAttr ".phl[3621]" 0;
	setAttr ".phl[3622]" 0;
	setAttr ".phl[3623]" 0;
	setAttr ".phl[3624]" 0;
	setAttr ".phl[3625]" 0;
	setAttr ".phl[3626]" 0;
	setAttr ".phl[3627]" 0;
	setAttr ".phl[3628]" 0;
	setAttr ".phl[3629]" 0;
	setAttr ".phl[3630]" 0;
	setAttr ".phl[3631]" 0;
	setAttr ".phl[3632]" 0;
	setAttr ".phl[3633]" 0;
	setAttr ".phl[3634]" 0;
	setAttr ".phl[3635]" 0;
	setAttr ".phl[3636]" 0;
	setAttr ".phl[3637]" 0;
	setAttr ".phl[3638]" 0;
	setAttr ".phl[3639]" 0;
	setAttr ".phl[3640]" 0;
	setAttr ".phl[3641]" 0;
	setAttr ".phl[3642]" 0;
	setAttr ".phl[3643]" 0;
	setAttr ".phl[3644]" 0;
	setAttr ".phl[3645]" 0;
	setAttr ".phl[3646]" 0;
	setAttr ".phl[3647]" 0;
	setAttr ".phl[3648]" 0;
	setAttr ".phl[3649]" 0;
	setAttr ".phl[3650]" 0;
	setAttr ".phl[3651]" 0;
	setAttr ".phl[3652]" 0;
	setAttr ".phl[3653]" 0;
	setAttr ".phl[3654]" 0;
	setAttr ".phl[3655]" 0;
	setAttr ".phl[3656]" 0;
	setAttr ".phl[3657]" 0;
	setAttr ".phl[3658]" 0;
	setAttr ".phl[3659]" 0;
	setAttr ".phl[3660]" 0;
	setAttr ".phl[3661]" 0;
	setAttr ".phl[3662]" 0;
	setAttr ".phl[3663]" 0;
	setAttr ".phl[3664]" 0;
	setAttr ".phl[3665]" 0;
	setAttr ".phl[3666]" 0;
	setAttr ".phl[3667]" 0;
	setAttr ".phl[3668]" 0;
	setAttr ".phl[3669]" 0;
	setAttr ".phl[3670]" 0;
	setAttr ".phl[3671]" 0;
	setAttr ".phl[3672]" 0;
	setAttr ".phl[3673]" 0;
	setAttr ".phl[3674]" 0;
	setAttr ".phl[3675]" 0;
	setAttr ".phl[3676]" 0;
	setAttr ".phl[3677]" 0;
	setAttr ".phl[3678]" 0;
	setAttr ".phl[3679]" 0;
	setAttr ".phl[3680]" 0;
	setAttr ".phl[3681]" 0;
	setAttr ".phl[3682]" 0;
	setAttr ".phl[3683]" 0;
	setAttr ".phl[3684]" 0;
	setAttr ".phl[3685]" 0;
	setAttr ".phl[3686]" 0;
	setAttr ".phl[3687]" 0;
	setAttr ".phl[3688]" 0;
	setAttr ".phl[3689]" 0;
	setAttr ".phl[3690]" 0;
	setAttr ".phl[3691]" 0;
	setAttr ".phl[3692]" 0;
	setAttr ".phl[3693]" 0;
	setAttr ".phl[3694]" 0;
	setAttr ".phl[3695]" 0;
	setAttr ".phl[3696]" 0;
	setAttr ".phl[3697]" 0;
	setAttr ".phl[3698]" 0;
	setAttr ".phl[3699]" 0;
	setAttr ".phl[3700]" 0;
	setAttr ".phl[3701]" 0;
	setAttr ".phl[3702]" 0;
	setAttr ".phl[3703]" 0;
	setAttr ".phl[3704]" 0;
	setAttr ".phl[3705]" 0;
	setAttr ".phl[3706]" 0;
	setAttr ".phl[3707]" 0;
	setAttr ".phl[3708]" 0;
	setAttr ".phl[3709]" 0;
	setAttr ".phl[3710]" 0;
	setAttr ".phl[3711]" 0;
	setAttr ".phl[3712]" 0;
	setAttr ".phl[3713]" 0;
	setAttr ".phl[3714]" 0;
	setAttr ".phl[3715]" 0;
	setAttr ".phl[3716]" 0;
	setAttr ".phl[3717]" 0;
	setAttr ".phl[3718]" 0;
	setAttr ".phl[3719]" 0;
	setAttr ".phl[3720]" 0;
	setAttr ".phl[3721]" 0;
	setAttr ".phl[3722]" 0;
	setAttr ".phl[3723]" 0;
	setAttr ".phl[3724]" 0;
	setAttr ".phl[3725]" 0;
	setAttr ".phl[3726]" 0;
	setAttr ".phl[3727]" 0;
	setAttr ".phl[3728]" 0;
	setAttr ".phl[3729]" 0;
	setAttr ".phl[3730]" 0;
	setAttr ".phl[3731]" 0;
	setAttr ".phl[3732]" 0;
	setAttr ".phl[3733]" 0;
	setAttr ".phl[3734]" 0;
	setAttr ".phl[3735]" 0;
	setAttr ".phl[3736]" 0;
	setAttr ".phl[3737]" 0;
	setAttr ".phl[3738]" 0;
	setAttr ".phl[3739]" 0;
	setAttr ".phl[3740]" 0;
	setAttr ".phl[3741]" 0;
	setAttr ".phl[3742]" 0;
	setAttr ".phl[3743]" 0;
	setAttr ".phl[3744]" 0;
	setAttr ".phl[3745]" 0;
	setAttr ".phl[3746]" 0;
	setAttr ".phl[3747]" 0;
	setAttr ".phl[3748]" 0;
	setAttr ".phl[3749]" 0;
	setAttr ".phl[3750]" 0;
	setAttr ".phl[3751]" 0;
	setAttr ".phl[3752]" 0;
	setAttr ".phl[3753]" 0;
	setAttr ".phl[3754]" 0;
	setAttr ".phl[3755]" 0;
	setAttr ".phl[3756]" 0;
	setAttr ".phl[3757]" 0;
	setAttr ".phl[3758]" 0;
	setAttr ".phl[3759]" 0;
	setAttr ".phl[3760]" 0;
	setAttr ".phl[3761]" 0;
	setAttr ".phl[3762]" 0;
	setAttr ".phl[3763]" 0;
	setAttr ".phl[3764]" 0;
	setAttr ".phl[3765]" 0;
	setAttr ".phl[3766]" 0;
	setAttr ".phl[3767]" 0;
	setAttr ".phl[3768]" 0;
	setAttr ".phl[3769]" 0;
	setAttr ".phl[3770]" 0;
	setAttr ".phl[3771]" 0;
	setAttr ".phl[3772]" 0;
	setAttr ".phl[3773]" 0;
	setAttr ".phl[3774]" 0;
	setAttr ".phl[3775]" 0;
	setAttr ".phl[3776]" 0;
	setAttr ".phl[3777]" 0;
	setAttr ".phl[3778]" 0;
	setAttr ".phl[3779]" 0;
	setAttr ".phl[3780]" 0;
	setAttr ".phl[3781]" 0;
	setAttr ".phl[3782]" 0;
	setAttr ".phl[3783]" 0;
	setAttr ".phl[3784]" 0;
	setAttr ".phl[3785]" 0;
	setAttr ".phl[3786]" 0;
	setAttr ".phl[3787]" 0;
	setAttr ".phl[3788]" 0;
	setAttr ".phl[3789]" 0;
	setAttr ".phl[3790]" 0;
	setAttr ".phl[3791]" 0;
	setAttr ".phl[3792]" 0;
	setAttr ".phl[3793]" 0;
	setAttr ".phl[3794]" 0;
	setAttr ".phl[3795]" 0;
	setAttr ".phl[3796]" 0;
	setAttr ".phl[3797]" 0;
	setAttr ".phl[3798]" 0;
	setAttr ".phl[3799]" 0;
	setAttr ".phl[3800]" 0;
	setAttr ".phl[3801]" 0;
	setAttr ".phl[3802]" 0;
	setAttr ".phl[3803]" 0;
	setAttr ".phl[3804]" 0;
	setAttr ".phl[3805]" 0;
	setAttr ".phl[3806]" 0;
	setAttr ".phl[3807]" 0;
	setAttr ".phl[3808]" 0;
	setAttr ".phl[3809]" 0;
	setAttr ".phl[3810]" 0;
	setAttr ".phl[3811]" 0;
	setAttr ".phl[3812]" 0;
	setAttr ".phl[3813]" 0;
	setAttr ".phl[3814]" 0;
	setAttr ".phl[3815]" 0;
	setAttr ".phl[3816]" 0;
	setAttr ".phl[3817]" 0;
	setAttr ".phl[3818]" 0;
	setAttr ".phl[3819]" 0;
	setAttr ".phl[3820]" 0;
	setAttr ".phl[3821]" 0;
	setAttr ".phl[3822]" 0;
	setAttr ".phl[3823]" 0;
	setAttr ".phl[3824]" 0;
	setAttr ".phl[3825]" 0;
	setAttr ".phl[3826]" 0;
	setAttr ".phl[3827]" 0;
	setAttr ".phl[3828]" 0;
	setAttr ".phl[3829]" 0;
	setAttr ".phl[3830]" 0;
	setAttr ".phl[3831]" 0;
	setAttr ".phl[3832]" 0;
	setAttr ".phl[3833]" 0;
	setAttr ".phl[3834]" 0;
	setAttr ".phl[3835]" 0;
	setAttr ".phl[3836]" 0;
	setAttr ".phl[3837]" 0;
	setAttr ".phl[3838]" 0;
	setAttr ".phl[3839]" 0;
	setAttr ".phl[3840]" 0;
	setAttr ".phl[3841]" 0;
	setAttr ".phl[3842]" 0;
	setAttr ".phl[3843]" 0;
	setAttr ".phl[3844]" 0;
	setAttr ".phl[3845]" 0;
	setAttr ".phl[3846]" 0;
	setAttr ".phl[3847]" 0;
	setAttr ".phl[3848]" 0;
	setAttr ".phl[3849]" 0;
	setAttr ".phl[3850]" 0;
	setAttr ".phl[3851]" 0;
	setAttr ".phl[3852]" 0;
	setAttr ".phl[3853]" 0;
	setAttr ".phl[3854]" 0;
	setAttr ".phl[3855]" 0;
	setAttr ".phl[3856]" 0;
	setAttr ".phl[3857]" 0;
	setAttr ".phl[3858]" 0;
	setAttr ".phl[3859]" 0;
	setAttr ".phl[3860]" 0;
	setAttr ".phl[3861]" 0;
	setAttr ".phl[3862]" 0;
	setAttr ".phl[3863]" 0;
	setAttr ".phl[3864]" 0;
	setAttr ".phl[3865]" 0;
	setAttr ".phl[3866]" 0;
	setAttr ".phl[3867]" 0;
	setAttr ".phl[3868]" 0;
	setAttr ".phl[3869]" 0;
	setAttr ".phl[3870]" 0;
	setAttr ".phl[3871]" 0;
	setAttr ".phl[3872]" 0;
	setAttr ".phl[3873]" 0;
	setAttr ".phl[3874]" 0;
	setAttr ".phl[3875]" 0;
	setAttr ".phl[3876]" 0;
	setAttr ".phl[3877]" 0;
	setAttr ".phl[3878]" 0;
	setAttr ".phl[3879]" 0;
	setAttr ".phl[3880]" 0;
	setAttr ".phl[3881]" 0;
	setAttr ".phl[3882]" 0;
	setAttr ".phl[3883]" 0;
	setAttr ".phl[3884]" 0;
	setAttr ".phl[3885]" 0;
	setAttr ".phl[3886]" 0;
	setAttr ".phl[3887]" 0;
	setAttr ".phl[3888]" 0;
	setAttr ".phl[3889]" 0;
	setAttr ".phl[3890]" 0;
	setAttr ".ed" -type "dataReferenceEdits" 
		"D_RN"
		"D_RN" 23
		1 |D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball "blendD:unitConversion1" 
		"blendD:unitConversion1" " -ci 1 -k 1 -bt \"aDBL\" -dv 1 -smn 0 -smx 1 -at \"double\""
		
		1 |D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe 
		"blendD:LFoot" "blendD:LFoot" " -ci 1 -k 1 -bt \"aDBL\" -dv 1 -smn 0 -smx 1 -at \"double\""
		
		1 |D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball "blendD:unitConversion2" 
		"blendD:unitConversion2" " -ci 1 -k 1 -bt \"aDBL\" -dv 1 -smn 0 -smx 1 -at \"double\""
		
		1 |D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe 
		"blendD:RFoot" "blendD:RFoot" " -ci 1 -k 1 -bt \"aDBL\" -dv 1 -smn 0 -smx 1 -at \"double\""
		
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"blendD:unitConversion1" " -k 1"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe" 
		"blendD:LFoot" " -k 1"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball" 
		"blendD:unitConversion2" " -k 1"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe" 
		"blendD:RFoot" " -k 1"
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.blendD:unitConversion1" 
		"D_RN.placeHolderList[1963]" ""
		5 3 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.blendD:unitConversion1" 
		"D_RN.placeHolderList[1964]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.blendD:LFoot" 
		"D_RN.placeHolderList[2014]" ""
		5 3 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.blendD:LFoot" 
		"D_RN.placeHolderList[2015]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.blendD:unitConversion2" 
		"D_RN.placeHolderList[2073]" ""
		5 3 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.blendD:unitConversion2" 
		"D_RN.placeHolderList[2074]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.blendD:RFoot" 
		"D_RN.placeHolderList[2124]" ""
		5 3 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.blendD:RFoot" 
		"D_RN.placeHolderList[2125]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[3459]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[3460]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[3461]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[3462]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[3463]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[3464]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[3465]" ""
		"D_RN" 662
		2 "|D_:Entity" "translate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity" "translateZ" " -av"
		2 "|D_:Entity" "rotate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity" "rotateX" " -av"
		2 "|D_:Entity" "rotateY" " -av"
		2 "|D_:Entity" "rotateZ" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "translate" " -type \"double3\" 0 0 0"
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
		"rotate" " -type \"double3\" 0 0 -61.397554"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotate" " -type \"double3\" 0 0 -37.526539"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotate" " -type \"double3\" 0 0 -69.876544"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotate" " -type \"double3\" 0 0 -44.659082"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotate" " -type \"double3\" -3.950977 -1.915783 -38.738955"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotate" " -type \"double3\" 0 0 -33.101591"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotate" " -type \"double3\" -18.444187 -17.180223 -9.279406"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotate" " -type \"double3\" 0 0 0"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotate" " -type \"double3\" 2.766407 -1.04906 -35.648591"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotate" " -type \"double3\" 0 0 -37.329396"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotate" " -type \"double3\" 1.710334 -2.34635 -31.866088"
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
		"rotate" " -type \"double3\" 3.952561 -5.595458 -32.21645"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotate" " -type \"double3\" 0 0 -45.116057"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotate" " -type \"double3\" 2.346419 -45.660733 -24.281669"
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
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translate" " -type \"double3\" 2.996434 0 -31.52703"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotate" " -type \"double3\" 0 5.166083 0"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "scale" " -type \"double3\" 1 1 1"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "scaleX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "scaleY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "scaleZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "BallRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"translate" " -type \"double3\" 0 0 -11.97981"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"rotate" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball" 
		"segmentScaleCompensate" " 1"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle" 
		"translate" " -type \"double3\" 0 13.613421 -6.171417"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle|D_:ikHandle_l_ankle_poleVectorConstraint1" 
		"nodeState" " -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle|D_:ikHandle_l_ankle_poleVectorConstraint1" 
		"offset" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle|D_:ikHandle_l_ankle_poleVectorConstraint1" 
		"L_KneeW0" " -k 1 1"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translate" " -type \"double3\" -4.264988 -1.006105 -59.355756"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotate" " -type \"double3\" 0 -29.070586 0"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "scale" " -type \"double3\" 1 1 1"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "scaleX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "scaleY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "scaleZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "BallRoll" " -av -k 1 19.411444"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base" "rotate" " -type \"double3\" 0 0 0"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base" "segmentScaleCompensate" 
		" 1"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball" 
		"rotate" " -type \"double3\" 17.690491 0.276776 -0.340263"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball" 
		"segmentScaleCompensate" " 1"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle|D_:ikHandle_r_ankle_poleVectorConstraint1" 
		"nodeState" " -k 1 0"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle|D_:ikHandle_r_ankle_poleVectorConstraint1" 
		"offset" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle|D_:ikHandle_r_ankle_poleVectorConstraint1" 
		"R_KneeW0" " -k 1 1"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translate" " -type \"double3\" 3.365302 1.435957 -26.64966"
		
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translate" " -type \"double3\" -6.915389 -1.281829 -28.756239"
		
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translate" " -type \"double3\" 0 -12.480157 -43.358819"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotate" " -type \"double3\" 51.331238 -3.456806 23.474883"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translate" " -type \"double3\" 0 0 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateX" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateY" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateZ" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotate" " -type \"double3\" 24.291217 -2.077235 0.106942"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateX" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateY" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateZ" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotate" " -type \"double3\" 2.818926 0.00841513 0.0197946"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translate" " -type \"double3\" -1.082054 -0.997747 -0.950109"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotate" " -type \"double3\" 0.830734 0.564259 -2.515764"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translate" " -type \"double3\" 0.00438577 0.779007 -0.00780013"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translate" " -type \"double3\" 0.78 1 -1.5"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translate" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotate" " -type \"double3\" -3.212818 -1.465578 1.762161"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"Mask" " -av -k 1 76.081551"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translate" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotate" " -type \"double3\" 18.812227 -43.221985 -39.252693"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotate" " -type \"double3\" -20.178409 -39.100883 22.566614"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotate" " -type \"double3\" -14.577768 -23.613183 -16.730429"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotate" " -type \"double3\" 10.473003 41.997321 34.402517"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotate" " -type \"double3\" -46.230704 32.983786 -20.505638"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotate" " -type \"double3\" -17.099179 28.242374 -101.696789"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotate" " -type \"double3\" 0 -1.304237 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:controlcurvesGRP_parentConstraint1" "nodeState" 
		" -k 1 0"
		2 "|D_:controlcurvesGRP|D_:controlcurvesGRP_parentConstraint1" "target[0].targetOffsetTranslate" 
		" -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:controlcurvesGRP_parentConstraint1" "target[0].targetOffsetRotate" 
		" -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:controlcurvesGRP_parentConstraint1" "interpType" 
		" 1"
		2 "|D_:controlcurvesGRP|D_:controlcurvesGRP_parentConstraint1" "DiverGlobalW0" 
		" -k 1 1"
		2 "|D_:controlcurvesGRP|D_:controlcurvesGRP_scaleConstraint1" "nodeState" 
		" -k 1 0"
		2 "|D_:controlcurvesGRP|D_:controlcurvesGRP_scaleConstraint1" "offset" " -type \"double3\" 1 1 1"
		
		2 "|D_:controlcurvesGRP|D_:controlcurvesGRP_scaleConstraint1" "DiverGlobalW0" 
		" -k 1 1"
		2 "D_:leftControl" "visibility" " 1"
		2 "D_:joints" "visibility" " 0"
		2 "D_:geometry" "displayType" " 2"
		2 "D_:geometry" "visibility" " 1"
		2 "D_:rightControl" "visibility" " 1"
		2 "D_:torso" "visibility" " 1"
		2 "D_:ReverseFeet" "visibility" " 0"
		2 "D_:skinCluster1" "nodeState" " 0"
		3 "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.translateY" 
		""
		3 "D_:unitConversion2.output" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.rotateX" 
		""
		3 "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.translateY" 
		""
		3 "D_:unitConversion1.output" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.rotateX" 
		""
		5 4 "D_RN" "|D_:Entity.translateX" "D_RN.placeHolderList[3466]" ""
		5 4 "D_RN" "|D_:Entity.translateY" "D_RN.placeHolderList[3467]" ""
		5 4 "D_RN" "|D_:Entity.translateZ" "D_RN.placeHolderList[3468]" ""
		5 4 "D_RN" "|D_:Entity.rotateX" "D_RN.placeHolderList[3469]" ""
		5 4 "D_RN" "|D_:Entity.rotateY" "D_RN.placeHolderList[3470]" ""
		5 4 "D_RN" "|D_:Entity.rotateZ" "D_RN.placeHolderList[3471]" ""
		5 4 "D_RN" "|D_:Entity.visibility" "D_RN.placeHolderList[3472]" ""
		5 4 "D_RN" "|D_:Entity.scaleX" "D_RN.placeHolderList[3473]" ""
		5 4 "D_RN" "|D_:Entity.scaleY" "D_RN.placeHolderList[3474]" ""
		5 4 "D_RN" "|D_:Entity.scaleZ" "D_RN.placeHolderList[3475]" ""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateX" "D_RN.placeHolderList[3476]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateY" "D_RN.placeHolderList[3477]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateZ" "D_RN.placeHolderList[3478]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateX" "D_RN.placeHolderList[3479]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateY" "D_RN.placeHolderList[3480]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateZ" "D_RN.placeHolderList[3481]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleX" "D_RN.placeHolderList[3482]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleY" "D_RN.placeHolderList[3483]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleZ" "D_RN.placeHolderList[3484]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.visibility" "D_RN.placeHolderList[3485]" 
		""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateX" 
		"D_RN.placeHolderList[3486]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateY" 
		"D_RN.placeHolderList[3487]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateZ" 
		"D_RN.placeHolderList[3488]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.visibility" 
		"D_RN.placeHolderList[3489]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateX" 
		"D_RN.placeHolderList[3490]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateY" 
		"D_RN.placeHolderList[3491]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateZ" 
		"D_RN.placeHolderList[3492]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.visibility" 
		"D_RN.placeHolderList[3493]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateX" 
		"D_RN.placeHolderList[3494]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateY" 
		"D_RN.placeHolderList[3495]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateZ" 
		"D_RN.placeHolderList[3496]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.visibility" 
		"D_RN.placeHolderList[3497]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateX" 
		"D_RN.placeHolderList[3498]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateY" 
		"D_RN.placeHolderList[3499]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateZ" 
		"D_RN.placeHolderList[3500]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.visibility" 
		"D_RN.placeHolderList[3501]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateX" 
		"D_RN.placeHolderList[3502]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateY" 
		"D_RN.placeHolderList[3503]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateZ" 
		"D_RN.placeHolderList[3504]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.visibility" 
		"D_RN.placeHolderList[3505]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateX" 
		"D_RN.placeHolderList[3506]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateY" 
		"D_RN.placeHolderList[3507]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateZ" 
		"D_RN.placeHolderList[3508]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.visibility" 
		"D_RN.placeHolderList[3509]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateX" 
		"D_RN.placeHolderList[3510]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateY" 
		"D_RN.placeHolderList[3511]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateZ" 
		"D_RN.placeHolderList[3512]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.visibility" 
		"D_RN.placeHolderList[3513]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateX" 
		"D_RN.placeHolderList[3514]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateY" 
		"D_RN.placeHolderList[3515]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateZ" 
		"D_RN.placeHolderList[3516]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.visibility" 
		"D_RN.placeHolderList[3517]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateX" 
		"D_RN.placeHolderList[3518]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateY" 
		"D_RN.placeHolderList[3519]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateZ" 
		"D_RN.placeHolderList[3520]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.visibility" 
		"D_RN.placeHolderList[3521]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateX" 
		"D_RN.placeHolderList[3522]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateY" 
		"D_RN.placeHolderList[3523]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateZ" 
		"D_RN.placeHolderList[3524]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.visibility" 
		"D_RN.placeHolderList[3525]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateX" 
		"D_RN.placeHolderList[3526]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateY" 
		"D_RN.placeHolderList[3527]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateZ" 
		"D_RN.placeHolderList[3528]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.visibility" 
		"D_RN.placeHolderList[3529]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateX" 
		"D_RN.placeHolderList[3530]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateY" 
		"D_RN.placeHolderList[3531]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateZ" 
		"D_RN.placeHolderList[3532]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.visibility" 
		"D_RN.placeHolderList[3533]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateX" 
		"D_RN.placeHolderList[3534]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateY" 
		"D_RN.placeHolderList[3535]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateZ" 
		"D_RN.placeHolderList[3536]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.visibility" 
		"D_RN.placeHolderList[3537]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateX" 
		"D_RN.placeHolderList[3538]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateY" 
		"D_RN.placeHolderList[3539]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateZ" 
		"D_RN.placeHolderList[3540]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.visibility" 
		"D_RN.placeHolderList[3541]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateX" 
		"D_RN.placeHolderList[3542]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateY" 
		"D_RN.placeHolderList[3543]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateZ" 
		"D_RN.placeHolderList[3544]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.visibility" 
		"D_RN.placeHolderList[3545]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateX" 
		"D_RN.placeHolderList[3546]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateY" 
		"D_RN.placeHolderList[3547]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateZ" 
		"D_RN.placeHolderList[3548]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.visibility" 
		"D_RN.placeHolderList[3549]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "D_RN.placeHolderList[3550]" 
		""
		5 3 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "D_RN.placeHolderList[3551]" 
		"D_:ikHandle_l_toe.ty"
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.BallRoll" "D_RN.placeHolderList[3552]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateX" "D_RN.placeHolderList[3553]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateY" "D_RN.placeHolderList[3554]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateZ" "D_RN.placeHolderList[3555]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateX" "D_RN.placeHolderList[3556]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateY" "D_RN.placeHolderList[3557]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateZ" "D_RN.placeHolderList[3558]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleX" "D_RN.placeHolderList[3559]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleY" "D_RN.placeHolderList[3560]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleZ" "D_RN.placeHolderList[3561]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.visibility" "D_RN.placeHolderList[3562]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.scaleX" "D_RN.placeHolderList[3563]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.scaleY" "D_RN.placeHolderList[3564]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.scaleZ" "D_RN.placeHolderList[3565]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.translateX" 
		"D_RN.placeHolderList[3566]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.translateY" 
		"D_RN.placeHolderList[3567]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.translateZ" 
		"D_RN.placeHolderList[3568]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.visibility" 
		"D_RN.placeHolderList[3569]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.rotateX" "D_RN.placeHolderList[3570]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.rotateY" "D_RN.placeHolderList[3571]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base.rotateZ" "D_RN.placeHolderList[3572]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.scaleX" 
		"D_RN.placeHolderList[3573]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.scaleY" 
		"D_RN.placeHolderList[3574]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.scaleZ" 
		"D_RN.placeHolderList[3575]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.translateX" 
		"D_RN.placeHolderList[3576]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.translateY" 
		"D_RN.placeHolderList[3577]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.translateZ" 
		"D_RN.placeHolderList[3578]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.visibility" 
		"D_RN.placeHolderList[3579]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.rotateX" 
		"D_RN.placeHolderList[3580]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.rotateY" 
		"D_RN.placeHolderList[3581]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe.rotateZ" 
		"D_RN.placeHolderList[3582]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.rotateX" 
		"D_RN.placeHolderList[3583]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.rotateY" 
		"D_RN.placeHolderList[3584]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.rotateZ" 
		"D_RN.placeHolderList[3585]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.scaleX" 
		"D_RN.placeHolderList[3586]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.scaleY" 
		"D_RN.placeHolderList[3587]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.scaleZ" 
		"D_RN.placeHolderList[3588]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.translateX" 
		"D_RN.placeHolderList[3589]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.translateY" 
		"D_RN.placeHolderList[3590]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.translateZ" 
		"D_RN.placeHolderList[3591]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball.visibility" 
		"D_RN.placeHolderList[3592]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.translateX" 
		"D_RN.placeHolderList[3593]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.translateY" 
		"D_RN.placeHolderList[3594]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.translateZ" 
		"D_RN.placeHolderList[3595]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.visibility" 
		"D_RN.placeHolderList[3596]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.rotateX" 
		"D_RN.placeHolderList[3597]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.rotateY" 
		"D_RN.placeHolderList[3598]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.rotateZ" 
		"D_RN.placeHolderList[3599]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.scaleX" 
		"D_RN.placeHolderList[3600]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.scaleY" 
		"D_RN.placeHolderList[3601]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle.scaleZ" 
		"D_RN.placeHolderList[3602]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.translateX" 
		"D_RN.placeHolderList[3603]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.translateY" 
		"D_RN.placeHolderList[3604]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.translateZ" 
		"D_RN.placeHolderList[3605]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.visibility" 
		"D_RN.placeHolderList[3606]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.rotateX" 
		"D_RN.placeHolderList[3607]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.rotateY" 
		"D_RN.placeHolderList[3608]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.rotateZ" 
		"D_RN.placeHolderList[3609]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.scaleX" 
		"D_RN.placeHolderList[3610]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.scaleY" 
		"D_RN.placeHolderList[3611]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.scaleZ" 
		"D_RN.placeHolderList[3612]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.offset" 
		"D_RN.placeHolderList[3613]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.roll" 
		"D_RN.placeHolderList[3614]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.twist" 
		"D_RN.placeHolderList[3615]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:L_RL_Ankle|D_:ikHandle_l_ankle.ikBlend" 
		"D_RN.placeHolderList[3616]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.translateX" 
		"D_RN.placeHolderList[3617]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.translateY" 
		"D_RN.placeHolderList[3618]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.translateZ" 
		"D_RN.placeHolderList[3619]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.visibility" 
		"D_RN.placeHolderList[3620]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.rotateX" 
		"D_RN.placeHolderList[3621]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.rotateY" 
		"D_RN.placeHolderList[3622]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.rotateZ" 
		"D_RN.placeHolderList[3623]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.scaleX" 
		"D_RN.placeHolderList[3624]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.scaleY" 
		"D_RN.placeHolderList[3625]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.scaleZ" 
		"D_RN.placeHolderList[3626]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.poleVectorX" 
		"D_RN.placeHolderList[3627]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.poleVectorY" 
		"D_RN.placeHolderList[3628]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.poleVectorZ" 
		"D_RN.placeHolderList[3629]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.offset" 
		"D_RN.placeHolderList[3630]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.roll" 
		"D_RN.placeHolderList[3631]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.twist" 
		"D_RN.placeHolderList[3632]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:L_RL_Ball|D_:ikHandle_l_ball.ikBlend" 
		"D_RN.placeHolderList[3633]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.translateY" 
		"D_RN.placeHolderList[3634]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.translateX" 
		"D_RN.placeHolderList[3635]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.translateZ" 
		"D_RN.placeHolderList[3636]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.visibility" 
		"D_RN.placeHolderList[3637]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.rotateX" 
		"D_RN.placeHolderList[3638]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.rotateY" 
		"D_RN.placeHolderList[3639]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.rotateZ" 
		"D_RN.placeHolderList[3640]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.scaleX" 
		"D_RN.placeHolderList[3641]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.scaleY" 
		"D_RN.placeHolderList[3642]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.scaleZ" 
		"D_RN.placeHolderList[3643]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.poleVectorX" 
		"D_RN.placeHolderList[3644]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.poleVectorY" 
		"D_RN.placeHolderList[3645]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.poleVectorZ" 
		"D_RN.placeHolderList[3646]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.offset" 
		"D_RN.placeHolderList[3647]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.roll" 
		"D_RN.placeHolderList[3648]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.twist" 
		"D_RN.placeHolderList[3649]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot|D_:L_RL_Base|D_:L_RL_Toe|D_:ikHandle_l_toe.ikBlend" 
		"D_RN.placeHolderList[3650]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "D_RN.placeHolderList[3651]" 
		""
		5 3 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "D_RN.placeHolderList[3652]" 
		"D_:ikHandle_r_toe.ty"
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.BallRoll" "D_RN.placeHolderList[3653]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateX" "D_RN.placeHolderList[3654]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateY" "D_RN.placeHolderList[3655]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateZ" "D_RN.placeHolderList[3656]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateX" "D_RN.placeHolderList[3657]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateY" "D_RN.placeHolderList[3658]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateZ" "D_RN.placeHolderList[3659]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleX" "D_RN.placeHolderList[3660]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleY" "D_RN.placeHolderList[3661]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleZ" "D_RN.placeHolderList[3662]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.visibility" "D_RN.placeHolderList[3663]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.scaleX" "D_RN.placeHolderList[3664]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.scaleY" "D_RN.placeHolderList[3665]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.scaleZ" "D_RN.placeHolderList[3666]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.rotateX" "D_RN.placeHolderList[3667]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.rotateY" "D_RN.placeHolderList[3668]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.rotateZ" "D_RN.placeHolderList[3669]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.translateX" 
		"D_RN.placeHolderList[3670]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.translateY" 
		"D_RN.placeHolderList[3671]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.translateZ" 
		"D_RN.placeHolderList[3672]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base.visibility" 
		"D_RN.placeHolderList[3673]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.scaleX" 
		"D_RN.placeHolderList[3674]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.scaleY" 
		"D_RN.placeHolderList[3675]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.scaleZ" 
		"D_RN.placeHolderList[3676]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.translateX" 
		"D_RN.placeHolderList[3677]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.translateY" 
		"D_RN.placeHolderList[3678]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.translateZ" 
		"D_RN.placeHolderList[3679]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.visibility" 
		"D_RN.placeHolderList[3680]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.rotateX" 
		"D_RN.placeHolderList[3681]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.rotateY" 
		"D_RN.placeHolderList[3682]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe.rotateZ" 
		"D_RN.placeHolderList[3683]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.rotateX" 
		"D_RN.placeHolderList[3684]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.rotateY" 
		"D_RN.placeHolderList[3685]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.rotateZ" 
		"D_RN.placeHolderList[3686]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.scaleX" 
		"D_RN.placeHolderList[3687]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.scaleY" 
		"D_RN.placeHolderList[3688]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.scaleZ" 
		"D_RN.placeHolderList[3689]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.translateX" 
		"D_RN.placeHolderList[3690]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.translateY" 
		"D_RN.placeHolderList[3691]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.translateZ" 
		"D_RN.placeHolderList[3692]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball.visibility" 
		"D_RN.placeHolderList[3693]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.translateX" 
		"D_RN.placeHolderList[3694]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.translateY" 
		"D_RN.placeHolderList[3695]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.translateZ" 
		"D_RN.placeHolderList[3696]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.visibility" 
		"D_RN.placeHolderList[3697]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.rotateX" 
		"D_RN.placeHolderList[3698]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.rotateY" 
		"D_RN.placeHolderList[3699]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.rotateZ" 
		"D_RN.placeHolderList[3700]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.scaleX" 
		"D_RN.placeHolderList[3701]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.scaleY" 
		"D_RN.placeHolderList[3702]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle.scaleZ" 
		"D_RN.placeHolderList[3703]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.translateX" 
		"D_RN.placeHolderList[3704]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.translateY" 
		"D_RN.placeHolderList[3705]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.translateZ" 
		"D_RN.placeHolderList[3706]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.visibility" 
		"D_RN.placeHolderList[3707]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.rotateX" 
		"D_RN.placeHolderList[3708]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.rotateY" 
		"D_RN.placeHolderList[3709]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.rotateZ" 
		"D_RN.placeHolderList[3710]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.scaleX" 
		"D_RN.placeHolderList[3711]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.scaleY" 
		"D_RN.placeHolderList[3712]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.scaleZ" 
		"D_RN.placeHolderList[3713]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.offset" 
		"D_RN.placeHolderList[3714]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.roll" 
		"D_RN.placeHolderList[3715]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.twist" 
		"D_RN.placeHolderList[3716]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:R_RL_Ankle|D_:ikHandle_r_ankle.ikBlend" 
		"D_RN.placeHolderList[3717]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.translateX" 
		"D_RN.placeHolderList[3718]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.translateY" 
		"D_RN.placeHolderList[3719]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.translateZ" 
		"D_RN.placeHolderList[3720]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.visibility" 
		"D_RN.placeHolderList[3721]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.rotateX" 
		"D_RN.placeHolderList[3722]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.rotateY" 
		"D_RN.placeHolderList[3723]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.rotateZ" 
		"D_RN.placeHolderList[3724]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.scaleX" 
		"D_RN.placeHolderList[3725]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.scaleY" 
		"D_RN.placeHolderList[3726]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.scaleZ" 
		"D_RN.placeHolderList[3727]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.poleVectorX" 
		"D_RN.placeHolderList[3728]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.poleVectorY" 
		"D_RN.placeHolderList[3729]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.poleVectorZ" 
		"D_RN.placeHolderList[3730]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.offset" 
		"D_RN.placeHolderList[3731]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.roll" 
		"D_RN.placeHolderList[3732]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.twist" 
		"D_RN.placeHolderList[3733]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:R_RL_Ball|D_:ikHandle_r_ball.ikBlend" 
		"D_RN.placeHolderList[3734]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.translateY" 
		"D_RN.placeHolderList[3735]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.translateX" 
		"D_RN.placeHolderList[3736]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.translateZ" 
		"D_RN.placeHolderList[3737]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.visibility" 
		"D_RN.placeHolderList[3738]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.rotateX" 
		"D_RN.placeHolderList[3739]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.rotateY" 
		"D_RN.placeHolderList[3740]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.rotateZ" 
		"D_RN.placeHolderList[3741]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.scaleX" 
		"D_RN.placeHolderList[3742]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.scaleY" 
		"D_RN.placeHolderList[3743]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.scaleZ" 
		"D_RN.placeHolderList[3744]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.poleVectorX" 
		"D_RN.placeHolderList[3745]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.poleVectorY" 
		"D_RN.placeHolderList[3746]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.poleVectorZ" 
		"D_RN.placeHolderList[3747]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.offset" 
		"D_RN.placeHolderList[3748]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.roll" 
		"D_RN.placeHolderList[3749]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.twist" 
		"D_RN.placeHolderList[3750]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot|D_:R_RL_Base|D_:R_RL_Toe|D_:ikHandle_r_toe.ikBlend" 
		"D_RN.placeHolderList[3751]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateX" "D_RN.placeHolderList[3752]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateY" "D_RN.placeHolderList[3753]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateZ" "D_RN.placeHolderList[3754]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleX" "D_RN.placeHolderList[3755]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleY" "D_RN.placeHolderList[3756]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleZ" "D_RN.placeHolderList[3757]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.visibility" "D_RN.placeHolderList[3758]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateX" "D_RN.placeHolderList[3759]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateY" "D_RN.placeHolderList[3760]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateZ" "D_RN.placeHolderList[3761]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleX" "D_RN.placeHolderList[3762]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleY" "D_RN.placeHolderList[3763]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleZ" "D_RN.placeHolderList[3764]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.visibility" "D_RN.placeHolderList[3765]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateX" "D_RN.placeHolderList[3766]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateY" "D_RN.placeHolderList[3767]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateZ" "D_RN.placeHolderList[3768]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateX" "D_RN.placeHolderList[3769]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateY" "D_RN.placeHolderList[3770]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateZ" "D_RN.placeHolderList[3771]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleX" "D_RN.placeHolderList[3772]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleY" "D_RN.placeHolderList[3773]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleZ" "D_RN.placeHolderList[3774]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.visibility" "D_RN.placeHolderList[3775]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateX" 
		"D_RN.placeHolderList[3776]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateY" 
		"D_RN.placeHolderList[3777]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateZ" 
		"D_RN.placeHolderList[3778]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleX" 
		"D_RN.placeHolderList[3779]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleY" 
		"D_RN.placeHolderList[3780]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleZ" 
		"D_RN.placeHolderList[3781]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateX" 
		"D_RN.placeHolderList[3782]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateY" 
		"D_RN.placeHolderList[3783]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateZ" 
		"D_RN.placeHolderList[3784]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.visibility" 
		"D_RN.placeHolderList[3785]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint.nodeState" 
		"D_RN.placeHolderList[3786]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint.target[0].targetWeight" 
		"D_RN.placeHolderList[3787]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint.offsetX" 
		"D_RN.placeHolderList[3788]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint.offsetY" 
		"D_RN.placeHolderList[3789]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint.offsetZ" 
		"D_RN.placeHolderList[3790]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint.spine0W0" 
		"D_RN.placeHolderList[3791]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleX" 
		"D_RN.placeHolderList[3792]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleY" 
		"D_RN.placeHolderList[3793]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleZ" 
		"D_RN.placeHolderList[3794]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateX" 
		"D_RN.placeHolderList[3795]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateY" 
		"D_RN.placeHolderList[3796]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateZ" 
		"D_RN.placeHolderList[3797]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.visibility" 
		"D_RN.placeHolderList[3798]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:Spine0Control1_pointConstraint.nodeState" 
		"D_RN.placeHolderList[3799]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:Spine0Control1_pointConstraint.target[0].targetWeight" 
		"D_RN.placeHolderList[3800]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:Spine0Control1_pointConstraint.offsetX" 
		"D_RN.placeHolderList[3801]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:Spine0Control1_pointConstraint.offsetY" 
		"D_RN.placeHolderList[3802]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:Spine0Control1_pointConstraint.offsetZ" 
		"D_RN.placeHolderList[3803]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:Spine0Control1_pointConstraint.spine1W0" 
		"D_RN.placeHolderList[3804]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateX" 
		"D_RN.placeHolderList[3805]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateY" 
		"D_RN.placeHolderList[3806]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateZ" 
		"D_RN.placeHolderList[3807]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateX" 
		"D_RN.placeHolderList[3808]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateY" 
		"D_RN.placeHolderList[3809]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateZ" 
		"D_RN.placeHolderList[3810]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.visibility" 
		"D_RN.placeHolderList[3811]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleX" 
		"D_RN.placeHolderList[3812]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleY" 
		"D_RN.placeHolderList[3813]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleZ" 
		"D_RN.placeHolderList[3814]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateX" 
		"D_RN.placeHolderList[3815]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateY" 
		"D_RN.placeHolderList[3816]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateZ" 
		"D_RN.placeHolderList[3817]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.visibility" 
		"D_RN.placeHolderList[3818]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleX" 
		"D_RN.placeHolderList[3819]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleY" 
		"D_RN.placeHolderList[3820]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleZ" 
		"D_RN.placeHolderList[3821]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateX" 
		"D_RN.placeHolderList[3822]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateY" 
		"D_RN.placeHolderList[3823]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateZ" 
		"D_RN.placeHolderList[3824]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.visibility" 
		"D_RN.placeHolderList[3825]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.Mask" 
		"D_RN.placeHolderList[3826]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateX" 
		"D_RN.placeHolderList[3827]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateY" 
		"D_RN.placeHolderList[3828]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateZ" 
		"D_RN.placeHolderList[3829]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.visibility" 
		"D_RN.placeHolderList[3830]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl|D_:NeckControl_pointConstraint.nodeState" 
		"D_RN.placeHolderList[3831]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl|D_:NeckControl_pointConstraint.target[0].targetWeight" 
		"D_RN.placeHolderList[3832]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl|D_:NeckControl_pointConstraint.offsetX" 
		"D_RN.placeHolderList[3833]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl|D_:NeckControl_pointConstraint.offsetY" 
		"D_RN.placeHolderList[3834]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl|D_:NeckControl_pointConstraint.offsetZ" 
		"D_RN.placeHolderList[3835]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl|D_:NeckControl_pointConstraint.neckW0" 
		"D_RN.placeHolderList[3836]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleX" 
		"D_RN.placeHolderList[3837]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleY" 
		"D_RN.placeHolderList[3838]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleZ" 
		"D_RN.placeHolderList[3839]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateX" 
		"D_RN.placeHolderList[3840]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateY" 
		"D_RN.placeHolderList[3841]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateZ" 
		"D_RN.placeHolderList[3842]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.visibility" 
		"D_RN.placeHolderList[3843]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateX" 
		"D_RN.placeHolderList[3844]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateY" 
		"D_RN.placeHolderList[3845]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateZ" 
		"D_RN.placeHolderList[3846]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.visibility" 
		"D_RN.placeHolderList[3847]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleX" 
		"D_RN.placeHolderList[3848]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleY" 
		"D_RN.placeHolderList[3849]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleZ" 
		"D_RN.placeHolderList[3850]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateX" 
		"D_RN.placeHolderList[3851]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateY" 
		"D_RN.placeHolderList[3852]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateZ" 
		"D_RN.placeHolderList[3853]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.visibility" 
		"D_RN.placeHolderList[3854]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleX" 
		"D_RN.placeHolderList[3855]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleY" 
		"D_RN.placeHolderList[3856]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleZ" 
		"D_RN.placeHolderList[3857]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateX" 
		"D_RN.placeHolderList[3858]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateY" 
		"D_RN.placeHolderList[3859]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateZ" 
		"D_RN.placeHolderList[3860]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.visibility" 
		"D_RN.placeHolderList[3861]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[3862]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[3863]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[3864]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[3865]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[3866]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[3867]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[3868]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleX" 
		"D_RN.placeHolderList[3869]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleY" 
		"D_RN.placeHolderList[3870]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleZ" 
		"D_RN.placeHolderList[3871]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateX" 
		"D_RN.placeHolderList[3872]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateY" 
		"D_RN.placeHolderList[3873]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateZ" 
		"D_RN.placeHolderList[3874]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.visibility" 
		"D_RN.placeHolderList[3875]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleX" 
		"D_RN.placeHolderList[3876]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleY" 
		"D_RN.placeHolderList[3877]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleZ" 
		"D_RN.placeHolderList[3878]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateX" 
		"D_RN.placeHolderList[3879]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateY" 
		"D_RN.placeHolderList[3880]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateZ" 
		"D_RN.placeHolderList[3881]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.visibility" 
		"D_RN.placeHolderList[3882]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl|D_:HipControl_pointConstraint.nodeState" 
		"D_RN.placeHolderList[3883]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl|D_:HipControl_pointConstraint.target[0].targetWeight" 
		"D_RN.placeHolderList[3884]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl|D_:HipControl_pointConstraint.offsetX" 
		"D_RN.placeHolderList[3885]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl|D_:HipControl_pointConstraint.offsetY" 
		"D_RN.placeHolderList[3886]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl|D_:HipControl_pointConstraint.offsetZ" 
		"D_RN.placeHolderList[3887]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl|D_:HipControl_pointConstraint.hipW0" 
		"D_RN.placeHolderList[3888]" ""
		5 3 "D_RN" "D_:unitConversion1.output" "D_RN.placeHolderList[3889]" 
		"D_:L_RL_Ball.rx"
		5 3 "D_RN" "D_:unitConversion2.output" "D_RN.placeHolderList[3890]" 
		"D_:R_RL_Ball.rx";
	setAttr ".ptag" -type "string" "";
lockNode;
createNode mentalrayItemsList -s -n "mentalrayItemsList";
createNode mentalrayGlobals -s -n "mentalrayGlobals";
createNode mentalrayOptions -s -n "miDefaultOptions";
	setAttr ".maxr" 2;
createNode mentalrayFramebuffer -s -n "miDefaultFramebuffer";
createNode script -n "uiConfigurationScriptNode";
	setAttr ".b" -type "string" (
		"// Maya Mel UI Configuration File.\n//\n//  This script is machine generated.  Edit at your own risk.\n//\n//\n\nglobal string $gMainPane;\nif (`paneLayout -exists $gMainPane`) {\n\n\tglobal int $gUseScenePanelConfig;\n\tint    $useSceneConfig = $gUseScenePanelConfig;\n\tint    $menusOkayInPanels = `optionVar -q allowMenusInPanels`;\tint    $nVisPanes = `paneLayout -q -nvp $gMainPane`;\n\tint    $nPanes = 0;\n\tstring $editorName;\n\tstring $panelName;\n\tstring $itemFilterName;\n\tstring $panelConfig;\n\n\t//\n\t//  get current state of the UI\n\t//\n\tsceneUIReplacement -update $gMainPane;\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Top View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Top View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"top\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n"
		+ "                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n"
		+ "                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n"
		+ "                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Top View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"top\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n"
		+ "            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n"
		+ "            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n"
		+ "\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Side View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Side View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"side\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n"
		+ "                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Side View\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"side\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n"
		+ "            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Front View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Front View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n"
		+ "                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n"
		+ "                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Front View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n"
		+ "            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n"
		+ "            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Persp View\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Persp View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n"
		+ "                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n"
		+ "                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Persp View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n"
		+ "        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n"
		+ "            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"outlinerPanel\" (localizedPanelLabel(\"Outliner\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\"Outliner\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            outlinerEditor -e \n                -showShapes 0\n                -showAttributes 0\n                -showConnected 0\n                -showAnimCurvesOnly 0\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 1\n                -ignoreDagHierarchy 0\n                -expandConnections 0\n"
		+ "                -showUnitlessCurves 1\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 0\n                -highlightActive 1\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"defaultSetFilter\" \n                -showSetMembers 1\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n"
		+ "\t\toutlinerPanel -edit -l (localizedPanelLabel(\"Outliner\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        outlinerEditor -e \n            -showShapes 0\n            -showAttributes 0\n            -showConnected 0\n            -showAnimCurvesOnly 0\n            -showMuteInfo 0\n            -autoExpand 0\n            -showDagOnly 1\n            -ignoreDagHierarchy 0\n            -expandConnections 0\n            -showUnitlessCurves 1\n            -showCompounds 1\n            -showLeafs 1\n            -showNumericAttrsOnly 0\n            -highlightActive 1\n            -autoSelectNewObjects 0\n            -doNotSelectNewObjects 0\n            -dropIsParent 1\n            -transmitFilters 0\n            -setFilter \"defaultSetFilter\" \n            -showSetMembers 1\n            -allowMultiSelection 1\n            -alwaysToggleSelect 0\n            -directSelect 0\n            -displayMode \"DAG\" \n            -expandObjects 0\n            -setsIgnoreFilters 1\n            -editAttrName 0\n            -showAttrValues 0\n"
		+ "            -highlightSecondary 0\n            -showUVAttrsOnly 0\n            -showTextureNodesOnly 0\n            -attrAlphaOrder \"default\" \n            -sortOrder \"none\" \n            -longNames 0\n            -niceNames 1\n            -showNamespace 1\n            $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"graphEditor\" (localizedPanelLabel(\"Graph Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"graphEditor\" -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 1\n"
		+ "                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n            animCurveEditor -e \n                -displayKeys 1\n"
		+ "                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 1\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                -clipTime \"on\" \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n"
		+ "                -expandConnections 1\n                -showUnitlessCurves 1\n                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n"
		+ "            animCurveEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 1\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                -clipTime \"on\" \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\tif ($useSceneConfig) {\n\t\tscriptedPanel -e -to $panelName;\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dopeSheetPanel\" (localizedPanelLabel(\"Dope Sheet\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dopeSheetPanel\" -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n"
		+ "                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n"
		+ "                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"clipEditorPanel\" (localizedPanelLabel(\"Trax Editor\")) `;\n"
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
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"scriptEditorPanel\" -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"outlinerPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            outlinerEditor -e \n                -showShapes 0\n                -showAttributes 0\n                -showConnected 0\n                -showAnimCurvesOnly 0\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 1\n                -ignoreDagHierarchy 0\n                -expandConnections 0\n                -showUnitlessCurves 1\n"
		+ "                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 0\n                -highlightActive 1\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"defaultSetFilter\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\toutlinerPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        outlinerEditor -e \n            -showShapes 0\n            -showAttributes 0\n            -showConnected 0\n            -showAnimCurvesOnly 0\n            -showMuteInfo 0\n            -autoExpand 0\n            -showDagOnly 1\n            -ignoreDagHierarchy 0\n            -expandConnections 0\n            -showUnitlessCurves 1\n            -showCompounds 0\n            -showLeafs 1\n            -showNumericAttrsOnly 0\n            -highlightActive 1\n            -autoSelectNewObjects 0\n            -doNotSelectNewObjects 0\n            -dropIsParent 1\n            -transmitFilters 0\n            -setFilter \"defaultSetFilter\" \n            -showSetMembers 0\n            -allowMultiSelection 1\n            -alwaysToggleSelect 0\n            -directSelect 0\n            -displayMode \"DAG\" \n            -expandObjects 0\n            -setsIgnoreFilters 1\n            -editAttrName 0\n            -showAttrValues 0\n            -highlightSecondary 0\n            -showUVAttrsOnly 0\n            -showTextureNodesOnly 0\n"
		+ "            -attrAlphaOrder \"default\" \n            -sortOrder \"none\" \n            -longNames 0\n            -niceNames 1\n            -showNamespace 1\n            $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"outlinerPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            outlinerEditor -e \n                -showShapes 0\n                -showAttributes 0\n                -showConnected 0\n                -showAnimCurvesOnly 0\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 1\n                -ignoreDagHierarchy 0\n                -expandConnections 0\n                -showUnitlessCurves 1\n                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 0\n                -highlightActive 1\n                -autoSelectNewObjects 0\n"
		+ "                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"defaultSetFilter\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\toutlinerPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        outlinerEditor -e \n            -showShapes 0\n            -showAttributes 0\n"
		+ "            -showConnected 0\n            -showAnimCurvesOnly 0\n            -showMuteInfo 0\n            -autoExpand 0\n            -showDagOnly 1\n            -ignoreDagHierarchy 0\n            -expandConnections 0\n            -showUnitlessCurves 1\n            -showCompounds 0\n            -showLeafs 1\n            -showNumericAttrsOnly 0\n            -highlightActive 1\n            -autoSelectNewObjects 0\n            -doNotSelectNewObjects 0\n            -dropIsParent 1\n            -transmitFilters 0\n            -setFilter \"defaultSetFilter\" \n            -showSetMembers 0\n            -allowMultiSelection 1\n            -alwaysToggleSelect 0\n            -directSelect 0\n            -displayMode \"DAG\" \n            -expandObjects 0\n            -setsIgnoreFilters 1\n            -editAttrName 0\n            -showAttrValues 0\n            -highlightSecondary 0\n            -showUVAttrsOnly 0\n            -showTextureNodesOnly 0\n            -attrAlphaOrder \"default\" \n            -sortOrder \"none\" \n            -longNames 0\n            -niceNames 1\n"
		+ "            -showNamespace 1\n            $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel16\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel16\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n"
		+ "                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n"
		+ "                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n"
		+ "\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel16\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n"
		+ "            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n"
		+ "            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel17\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel17\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n"
		+ "                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n"
		+ "                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel17\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n"
		+ "            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n"
		+ "            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel18\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel18\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n"
		+ "                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n"
		+ "                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel18\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n"
		+ "            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n"
		+ "            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel19\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel19\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n"
		+ "                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n"
		+ "                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n"
		+ "                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel19\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n"
		+ "            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n"
		+ "            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel20\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel20\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n"
		+ "                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n"
		+ "                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n"
		+ "                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel20\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n"
		+ "            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n"
		+ "            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel21\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel21\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n"
		+ "                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel21\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel22\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel22\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n"
		+ "                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n"
		+ "                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n"
		+ "                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel22\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n"
		+ "            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n"
		+ "            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel23\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel23\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n"
		+ "                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n"
		+ "                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel23\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n"
		+ "            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n"
		+ "            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n"
		+ "            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel24\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel24\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n"
		+ "                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n"
		+ "                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel24\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n"
		+ "            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n"
		+ "            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel25\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel25\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n"
		+ "                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n"
		+ "                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n"
		+ "                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 1\n                -manipulators 1\n                -dimensions 0\n                -handles 1\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel25\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n"
		+ "            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n"
		+ "            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 1\n            -manipulators 1\n            -dimensions 0\n            -handles 1\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel26\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel26\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n"
		+ "                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n"
		+ "                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel26\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n"
		+ "            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n"
		+ "            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel27\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel27\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n"
		+ "                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n"
		+ "                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n"
		+ "                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel27\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n"
		+ "            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n"
		+ "            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n"
		+ "                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n"
		+ "                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n"
		+ "                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n"
		+ "            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n"
		+ "            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n"
		+ "\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n"
		+ "                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n"
		+ "                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n"
		+ "        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n"
		+ "            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n"
		+ "            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n"
		+ "                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n"
		+ "                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n"
		+ "                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n"
		+ "            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n"
		+ "            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n"
		+ "                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n"
		+ "                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n"
		+ "                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n"
		+ "            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n"
		+ "            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n"
		+ "                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n"
		+ "                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n"
		+ "                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n"
		+ "                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n"
		+ "                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n"
		+ "            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n"
		+ "            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n"
		+ "            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n"
		+ "                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n"
		+ "                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n"
		+ "            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n"
		+ "            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\n"
		+ "modelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n"
		+ "                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n"
		+ "                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n"
		+ "                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n"
		+ "                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n"
		+ "            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n"
		+ "            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n"
		+ "            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n"
		+ "                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n"
		+ "                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n"
		+ "            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n"
		+ "            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\n"
		+ "modelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel28\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel28\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n"
		+ "                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n"
		+ "                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n"
		+ "\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel28\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n"
		+ "            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n"
		+ "            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel29\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel29\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n"
		+ "                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n"
		+ "                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel29\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n"
		+ "            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n"
		+ "            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel30\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel30\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n"
		+ "                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n"
		+ "                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel30\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n"
		+ "            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n"
		+ "            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel31\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel31\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n"
		+ "                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n"
		+ "                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n"
		+ "                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel31\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n"
		+ "            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n"
		+ "            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel32\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel32\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n"
		+ "                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n"
		+ "                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 0\n"
		+ "                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel32\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n"
		+ "            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n"
		+ "            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel33\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel33\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n"
		+ "                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel33\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel40\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel40\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n"
		+ "                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n"
		+ "                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n"
		+ "                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel40\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n"
		+ "            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n"
		+ "            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel41\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel41\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n"
		+ "                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n"
		+ "                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel41\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n"
		+ "            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n"
		+ "            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n"
		+ "            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel42\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel42\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n"
		+ "                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n"
		+ "                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel42\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n"
		+ "            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n"
		+ "            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel43\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel43\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n"
		+ "                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n"
		+ "                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n"
		+ "                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel43\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n"
		+ "            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n"
		+ "            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel44\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel44\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n"
		+ "                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n"
		+ "                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel44\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n"
		+ "            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n"
		+ "            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n"
		+ "                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n"
		+ "                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n"
		+ "                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n"
		+ "            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n"
		+ "            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel46\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel46\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n"
		+ "                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n"
		+ "                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n"
		+ "                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel46\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n"
		+ "            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n"
		+ "            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel47\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel47\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n"
		+ "                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n"
		+ "                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel47\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n"
		+ "            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n"
		+ "            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel48\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel48\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n"
		+ "                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n"
		+ "                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n"
		+ "                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel48\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n"
		+ "            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n"
		+ "            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel49\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel49\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n"
		+ "                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n"
		+ "                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n"
		+ "                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel49\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n"
		+ "            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n"
		+ "            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel53\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel53\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n"
		+ "                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 1\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel53\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n"
		+ "            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 1\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel54\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel54\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n"
		+ "                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n"
		+ "                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 1\n"
		+ "                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel54\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n"
		+ "            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n"
		+ "            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 1\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel55\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel55\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 0\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n"
		+ "                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n"
		+ "                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 0\n                -deformers 0\n                -dynamics 1\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 1\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel55\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n"
		+ "            -headsUpDisplay 0\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n"
		+ "            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 0\n            -deformers 0\n            -dynamics 1\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 1\n            -strokes 0\n            -shadows 0\n"
		+ "            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel60\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel60\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n"
		+ "                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n"
		+ "                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel60\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n"
		+ "            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n"
		+ "            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n"
		+ "                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n"
		+ "                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n"
		+ "                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n"
		+ "            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n"
		+ "            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\tif ($useSceneConfig) {\n        string $configName = `getPanel -cwl (localizedPanelLabel(\"Current Layout\"))`;\n"
		+ "        if (\"\" != $configName) {\n\t\t\tpanelConfiguration -edit -label (localizedPanelLabel(\"Current Layout\")) \n\t\t\t\t-defaultImage \"\"\n\t\t\t\t-image \"\"\n\t\t\t\t-sc false\n\t\t\t\t-configString \"global string $gMainPane; paneLayout -e -cn \\\"single\\\" -ps 1 100 100 $gMainPane;\"\n\t\t\t\t-removeAllPanels\n\t\t\t\t-ap false\n\t\t\t\t\t(localizedPanelLabel(\"Persp View\")) \n\t\t\t\t\t\"modelPanel\"\n"
		+ "\t\t\t\t\t\"$panelName = `modelPanel -unParent -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels `;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 0\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t\t\"modelPanel -edit -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels  $panelName;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 0\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t$configName;\n\n            setNamedPanelLayout (localizedPanelLabel(\"Current Layout\"));\n        }\n\n        panelHistory -e -clear mainPanelHistory;\n        setFocus `paneLayout -q -p1 $gMainPane`;\n        sceneUIReplacement -deleteRemaining;\n        sceneUIReplacement -clear;\n\t}\n\n\ngrid -spacing 500 -size 5000 -divisions 1 -displayAxes yes -displayGridLines yes -displayDivisionLines yes -displayPerspectiveLabels no -displayOrthographicLabels no -displayAxesBold yes -perspectiveLabelPosition axis -orthographicLabelPosition edge;\n}\n");
	setAttr ".st" 3;
createNode script -n "sceneConfigurationScriptNode";
	setAttr ".b" -type "string" "playbackOptions -min 0 -max 61 -ast 0 -aet 61 ";
	setAttr ".st" 6;
createNode animCurveTL -n "D_:L_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 2.9964341932866638 2 2.9964341932866638 
		5 2.9964341932866638 7 2.9964341932866638 9 2.9964341932866638 12 2.9964341932866638 
		14 2.9964341932866638 18 2.9964341932866638 20 2.9964341932866638 23 2.9964341932866638 
		26 2.9964341932866638 28 2.9964341932866638 33 2.9964341932866638 34 2.9964341932866638 
		38 2.9964341932866638 40 2.9964341932866638 42 2.9964341932866638 43 2.9964341932866638 
		47 2.9964341932866638 51 2.9964341932866638 52 2.9964341932866638 58 2.9964341932866638 
		61 2.9964341932866638;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTL -n "D_:RootControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:Spine0Control_translateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -0.0004640926137400277 2 -0.0004640926137400277 
		5 -0.0004640926137400277 7 -0.0004640926137400277 9 0.78 12 0.78 14 0.78 18 0.78 
		20 0.78 23 0.8531684818856784 26 0.78 28 -0.0004640926137400277 33 -0.0004640926137400277 
		34 -0.0004640926137400277 38 -0.0004640926137400277 40 -0.0004640926137400277 42 
		-0.0004640926137400277 43 -0.0004640926137400277 48 -0.0004640926137400277 51 -0.0004640926137400277 
		52 -0.0004640926137400277 58 -0.0004640926137400277 61 -0.0004640926137400277;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:L_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.0043857699112451395 2 0.0043857699112451395 
		5 0.0043857699112451395 7 0.0043857699112451395 9 0.0043857699112451395 12 0.0043857699112451395 
		14 0.0043857699112451395 18 0.0043857699112451395 20 0.0043857699112451395 23 0.0043857699112451395 
		26 0.0043857699112451395 28 0.0043857699112451395 33 0.0043857699112451395 34 0.0043857699112451395 
		38 0.0043857699112451395 40 0.0043857699112451395 42 0.0043857699112451395 43 0.0043857699112451395 
		48 0.0043857699112451395 51 0.0043857699112451395 52 0.0043857699112451395 58 0.0043857699112451395 
		61 0.0043857699112451395;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:TankControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.0025643796042819182 2 0.0025643796042819182 
		5 2.0113032417645438 7 0.86210734777910492 9 0.0025643796042819182 10 -1.0820541872008891 
		14 -1.0820541872008891 18 -1.0820541872008891 20 -1.0820541872008891 23 -0.53974508562167034 
		26 0.0025643796042819182 28 0.0025643796042819182 33 0.0025643796042819182 34 0.0025643796042819182 
		38 0.0025643796042819182 40 0.0025643796042819182 42 0.0025643796042819182 43 0.0025643796042819182 
		48 0.0025643796042819182 51 0.0025643796042819182 52 0.0025643796042819182 58 0.0025643796042819182 
		61 0.0025643796042819182;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:R_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -6.915388563106255 2 -6.915388563106255 
		5 -6.915388563106255 7 -6.915388563106255 9 -6.915388563106255 12 -6.915388563106255 
		14 -6.915388563106255 18 -6.915388563106255 20 -6.915388563106255 23 -14.617606125673557 
		26 -22.6823595538321 28 -24.615856574217883 33 -17.775655984440796 34 -16.893472013670408 
		38 -15.024142328436717 40 -13.442327080893621 42 -11.441936867459468 43 -10.633624670253536 
		48 -6.915388563106255 51 -6.915388563106255 52 -6.915388563106255 58 -6.915388563106255 
		61 -6.915388563106255;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:L_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 9.4513480500108429 2 2.9573593441525325 
		5 3.3711774951649089 7 3.3711774951649089 9 3.3711774951649089 12 3.3711774951649089 
		14 3.3711774951649089 18 3.3711774951649089 20 3.4416870095057024 23 3.7109462070386798 
		26 4.0621792974251871 28 4.5698820004582821 33 5.8546750423865976 34 6.0907073131769662 
		38 6.8355230396527356 40 7.0397753165494565 42 7.1881906682163006 43 7.3606444935245063 
		48 8.0438035870408573 51 8.4091199738182389 52 8.4091199738182389 58 9.2022451138760246 
		61 9.4513480500108429;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:R_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -4.2649879960397659 2 -4.2649879960397659 
		5 -4.2649879960397659 7 -4.2649879960397659 9 -4.2649879960397659 12 -4.2649879960397659 
		14 -4.2649879960397659 18 -4.2649879960397659 20 -4.2649879960397659 23 -4.2649879960397659 
		26 -4.2649879960397659 28 -4.2649879960397659 32 -4.2649879960397659 33 -4.2649879960397659 
		36 -4.2649879960397659 40 -4.2649879960397659 42 -4.2649879960397659 43 -4.2649879960397659 
		48 -4.2649879960397659 51 -4.2649879960397659 52 -4.2649879960397659 58 -4.2649879960397659 
		61 -4.2649879960397659;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:L_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 6.9075650151258792 5 0 7 0 9 0 12 
		0 14 0 18 0 20 0 23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0.47172083684784499 43 1.1372007651610307 
		47 2.2582145566082965 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTL -n "D_:RootControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -5.5464221608478859 2 -2.8306469770428695 
		5 -12.480156821808599 7 -12.480156821808599 9 -12.480156821808599 12 -12.480156821808599 
		14 -12.480156821808599 18 -12.480156821808599 20 -12.480156821808599 23 -12.460517558777504 
		26 -12.480156821808599 28 -12.689642358027633 33 -9.6771436188888167 34 -9.2305809556141227 
		38 -8.220084962763158 40 -7.67032565847793 42 -6.8610848723057272 43 -6.1630953906026065 
		48 -6.4267265515560634 51 -5.4220095550307095 52 -5.4220095550307095 58 -5.4916984585485169 
		61 -5.5464221608478859;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:Spine0Control_translateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -3.5527136788005009e-015;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.6978503469639965 2 0.6978503469639965 
		5 0.6978503469639965 7 0.6978503469639965 9 1 12 1 14 1 18 1 20 1 23 1.0283265215223614 
		26 1 28 0.6978503469639965 33 0.6978503469639965 34 0.6978503469639965 38 0.6978503469639965 
		40 0.6978503469639965 42 0.6978503469639965 43 0.6978503469639965 48 0.6978503469639965 
		51 0.6978503469639965 52 0.6978503469639965 58 0.6978503469639965 61 0.6978503469639965;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:L_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.77900741554026665 2 0.77900741554026665 
		5 0.77900741554026665 7 0.77900741554026665 9 0.77900741554026665 12 0.77900741554026665 
		14 0.77900741554026665 18 0.77900741554026665 20 0.77900741554026665 23 0.77900741554026665 
		26 0.77900741554026665 28 0.77900741554026665 33 0.77900741554026665 34 0.77900741554026665 
		38 0.77900741554026665 40 0.77900741554026665 42 0.77900741554026665 43 0.77900741554026665 
		48 0.77900741554026665 51 0.77900741554026665 52 0.77900741554026665 58 0.77900741554026665 
		61 0.77900741554026665;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:TankControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -0.045469619462510075 2 -0.045469619462510075 
		5 2.3537334671739263 7 0.66465043431191151 9 -0.045469619462510075 10 -0.9977470189098756 
		14 -0.9977470189098756 18 -0.9977470189098756 20 -0.9977470189098756 23 -0.52160847882414174 
		26 -0.045469619462510075 28 -0.045469619462510075 33 -0.045469619462510075 34 -0.045469619462510075 
		38 -0.045469619462510075 40 -0.045469619462510075 42 -0.045469619462510075 43 -0.045469619462510075 
		48 -0.045469619462510075 51 -0.045469619462510075 52 -0.045469619462510075 58 -0.045469619462510075 
		61 -0.045469619462510075;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:R_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -1.3677491493794491 5 -1.2805917104491413 
		7 -1.2805917104491413 9 -1.2805917104491413 12 -1.2805917104491413 14 -1.2805917104491413 
		18 -1.2805917104491413 20 -1.2657411580112632 23 -3.8779547188738288 26 -5.6300864011541947 
		28 -1.0281233031891135 33 -0.75752309333208068 34 -0.70210530500146628 38 -0.55093911503520698 
		40 -0.50791996578926057 42 -0.47666106504023392 43 -0.44033923649009399 48 -0.29645381758763667 
		51 -0.21951171066085781 52 -0.21951171066085781 58 -0.052465495915206412 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:L_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 6.7130214314034404 5 1.4345706872185688 
		7 1.4345706872185688 9 1.4345706872185688 12 1.4345706872185688 14 1.4345706872185688 
		18 1.4345706872185688 20 1.4179344967371754 23 1.3544048119579326 26 1.2715340009410701 
		28 1.1517453564898321 33 0.84860806227425789 34 0.78652681359780974 38 0.6171843256972922 
		40 0.56899253423886098 42 0.53397504436458521 43 0.49328585806371061 48 0.33209958354774949 
		51 0.24590591784930593 52 0.24590591784930593 58 0.058773973553653269 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:R_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 8.9320213590771456 5 -0.291422304641138 
		7 -0.99317317768570912 9 -0.99317317768570912 12 -0.99317317768570912 14 -0.99317317768570912 
		18 -0.99317317768570912 20 -0.83798986867231706 23 0.15001698514412254 26 1.1027850180489871 
		28 1.0700244844129436 32 0.99423138820816437 33 -0.046896962748892657 36 0 40 0 42 
		0 43 0 48 0 51 0 52 0 58 1.4439719534152902 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:L_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -2.448549867634112 2 -2.7248119455167661 
		5 -14.061739657537593 7 -29.264028572348327 9 -31.527029537858102 12 -31.527029537858102 
		14 -31.527029537858102 18 -31.527029537858102 20 -31.527029537858102 23 -31.527029537858102 
		26 -31.527029537858102 28 -31.527029537858102 33 -31.527029537858102 34 -31.527029537858102 
		38 -31.527029537858102 40 -31.527029537858102 42 -29.06936352122419 43 -23.801755945322242 
		47 -10.228083237503498 51 -2.448549867634112 52 -2.448549867634112 58 -2.448549867634112 
		61 -2.448549867634112;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTL -n "D_:RootControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -0.60609539862080597 2 -32.387904537798406 
		5 -43.589839704208117 7 -43.683069487697495 9 -43.718301626757281 12 -43.52892384310627 
		14 -43.398069494225467 18 -43.341376864967728 20 -43.341376864967728 23 -43.024861386221382 
		26 -43.341376864967728 28 -46.717542875377042 33 -40.486359078670226 34 -39.160770191568261 
		38 -35.935787733476339 40 -33.638612396202291 42 -33.1229256437258 43 -32.427527639073709 
		48 -29.22873660567965 51 -23.086630468971645 52 -19.883213162248094 58 -4.5895915363488786 
		61 -0.60609539862080597;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:Spine0Control_translateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -1.1102230246251565e-016;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -0.013092045525845527 2 -0.013092045525845527 
		5 -0.013092045525845527 7 -0.013092045525845527 9 -1.5 12 -1.5 14 -1.5 18 -1.5 20 
		-1.5 23 -1.6393975883701659 26 -1.5 28 -0.013092045525845527 33 -0.013092045525845527 
		34 -0.013092045525845527 38 -0.013092045525845527 40 -0.013092045525845527 42 -0.013092045525845527 
		43 -0.013092045525845527 48 -0.013092045525845527 51 -0.013092045525845527 52 -0.013092045525845527 
		58 -0.013092045525845527 61 -0.013092045525845527;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:L_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -0.0078001293990800948 2 -0.0078001293990800948 
		5 -0.0078001293990800948 7 -0.0078001293990800948 9 -0.0078001293990800948 12 -0.0078001293990800948 
		14 -0.0078001293990800948 18 -0.0078001293990800948 20 -0.0078001293990800948 23 
		-0.0078001293990800948 26 -0.0078001293990800948 28 -0.0078001293990800948 33 -0.0078001293990800948 
		34 -0.0078001293990800948 38 -0.0078001293990800948 40 -0.0078001293990800948 42 
		-0.0078001293990800948 43 -0.0078001293990800948 48 -0.0078001293990800948 51 -0.0078001293990800948 
		52 -0.0078001293990800948 58 -0.0078001293990800948 61 -0.0078001293990800948;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:TankControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.0055634826900936782 2 0.0055634826900936782 
		5 -2.4606511187730997 7 -2.7827084608528172 9 0.0055634826900936782 10 -0.95010922032925937 
		14 -0.95010922032925937 18 -0.95010922032925937 20 -0.95010922032925937 23 -0.47227302902671292 
		26 0.0055634826900936782 28 0.0055634826900936782 33 0.0055634826900936782 34 0.0055634826900936782 
		38 0.0055634826900936782 40 0.0055634826900936782 42 0.0055634826900936782 43 0.0055634826900936782 
		48 0.0055634826900936782 51 0.0055634826900936782 52 0.0055634826900936782 58 0.0055634826900936782 
		61 0.0055634826900936782;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:R_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -26.332745545450685 5 -28.739800704623914 
		7 -28.739800704623914 9 -28.739800704623914 12 -28.739800704623914 14 -28.739800704623914 
		18 -28.739800704623914 20 -28.54253952862118 23 -27.731433555690021 26 -26.344120016984125 
		28 -23.07375463252167 33 -9.3234424222075525 34 -7.7667354102195798 38 -5.427027503177956 
		40 -4.7997983365804142 42 -4.5525603942754396 43 -4.272410689154702 48 -2.7716675082144491 
		51 -1.9847087150940892 52 -1.9847087150940892 58 -0.4659682172556181 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:L_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -38.135674287086125 5 -26.62393070636077 
		7 -26.62393070636077 9 -26.62393070636077 12 -26.62393070636077 14 -26.62393070636077 
		18 -26.62393070636077 20 -26.315182746727544 23 -25.136147141377727 26 -23.598163155706608 
		28 -21.37502775949504 33 -15.749159191073176 34 -14.715617982838227 38 -11.454209164587208 
		40 -10.559826633722697 42 -9.9099436460614623 43 -9.154800595245371 48 -6.1633744319430654 
		51 -4.5637222044038479 52 -4.5637222044038479 58 -1.0907752511320514 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:R_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 11.090879111775891 2 -18.997559172492664 
		5 -42.925459630284273 7 -57.059562198567434 9 -59.322563164077209 12 -59.322563164077209 
		14 -59.322563164077209 18 -59.322563164077209 20 -58.92425147401746 23 -57.196916103150222 
		26 -53.768843250904141 28 -44.299938333798686 32 -30.15933286754818 33 -26.106890771751505 
		36 -18.45740131088219 40 -18.45740131088219 42 -18.45740131088219 43 -18.45740131088219 
		48 -18.45740131088219 51 -18.45740131088219 52 -18.45740131088219 58 3.3108716717302791 
		61 11.090879111775891;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:L_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -11.198227829766967 5 0 7 0 9 0 12 
		0 14 0 18 0 20 0 23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0.69049481158467252 43 1.2697197000137206 
		47 -5.2941428727722961 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTA -n "D_:RootControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1.2910907900570192 2 39.216054503107735 
		5 43.069896055550224 7 49.334499850844722 9 50.731510004736883 12 51.277416092785451 
		14 51.404830458696992 18 50.465081152147164 20 45.837107025536007 23 42.582001299260789 
		26 39.908731836071958 28 38.383893576698597 33 32.937119515089549 34 31.657016666344383 
		38 27.621230400018831 40 29.183301113258832 42 30.411711201034464 43 30.94163487361574 
		48 20.886620988843926 51 17.796496697950989 52 15.968240937577136 58 4.4790332197829938 
		61 1.2910907900570192;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 9 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 9 9 9 10;
createNode animCurveTA -n "D_:Spine0Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.14113566014355422 2 12.134293682830489 
		5 11.987033709238558 7 20.47005040536202 9 23.696501574171414 12 24.302163705116204 
		14 24.302163705116204 18 24.302163705116204 20 24.433526424790209 23 25.936742069851714 
		26 25.142885111029834 28 20.226255205666686 33 7.3204369558006546 34 4.0143377047618998 
		38 0.97966438735636463 40 6.6605724194915163 42 5.161650361390115 43 2.9976318153829107 
		48 10.875259306418309 51 8.9821585425887882 52 6.6239968349634939 58 0.13010942486498714 
		61 0.14113566014355422;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:Spine1Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.55216102146285018 2 2.9732023912354029 
		5 2.8189257350010073 7 2.8189257350010073 9 2.8189257350010073 12 2.8189257350010073 
		14 2.8189257350010073 18 2.8189257350010073 20 2.8189257350010073 23 2.8189257350010073 
		26 2.8189257350010073 28 6.5267335369461463 33 9.5556147235065971 34 9.28311860278823 
		38 12.720042103148176 40 7.1667413418586525 42 7.2198819808623247 43 9.7219215089481246 
		48 2.7250048630074115 51 0.27608065883253435 52 0.27608065883253435 58 0.50902339537947239 
		61 0.55216102146285018;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:HeadControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.68228131693915994 2 0.46696422193830939 
		5 -0.19062978792480159 7 6.3040166244044542 9 -3.3036307014160777 12 -3.3036307014160777 
		14 -3.3036307014160777 18 -3.3036307014160777 20 -4.3933865347250203 23 -8.3531677245414215 
		26 -12.369457057302743 28 -13.760575651111628 33 -14.782230406658242 34 -14.548198019687851 
		38 -12.680729317337393 40 -12.099805521587189 42 -11.46153134769712 43 -10.617860678313102 
		48 -7.1569355861540807 51 -5.2432087913914698 52 -5.2432087913914698 58 -0.75088429063845563 
		61 0.68228131693915994;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:RShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -8.1378969009838897 2 -24.626618498115349 
		5 -31.28727104060123 7 -12.203272222092631 9 10.454180220152352 12 12.007834024325264 
		14 10.454180220152352 18 10.454180220152352 20 8.674656134041733 23 2.168693824013634 
		26 -4.6684111520850928 28 -8.2140062708383859 33 -5.0207680954530156 34 -2.7879353433794081 
		38 -0.46973943590285211 40 -0.45551773353381192 42 -2.6983305697791806 43 -4.1239290062398286 
		48 -7.9220417907393621 51 -9.0409538867280492 52 -9.0409538867280492 58 -8.2789997820865828 
		61 -8.1378969009838897;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 10 9 9 
		9 10 9 9 9 9 9 9 9 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 10 9 9 
		9 10 9 9 9 9 9 9 9 9 9 10 9 9 10;
createNode animCurveTA -n "D_:RElbowFK_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 -5.1130316113397329 5 -11.860848816070076 
		7 -27.566038086800521 9 -46.230703943718815 12 -46.230703943718815 18 -46.230703943718815 
		26 -18.064970940990229 28 -5.4742380161584441 31 4.08779685430369 36 3.7473093297669116 
		38 5.0567457866405876 42 10.094272294128698 46 6.2141724995944161 51 2.6460817609428608 
		56 0.33298701867378883 58 0;
	setAttr -s 17 ".kit[11:16]"  1 10 1 10 10 10;
	setAttr -s 17 ".kot[11:16]"  1 10 1 10 10 10;
	setAttr -s 17 ".kix[11:16]"  0.86212384700775146 0.9971429705619812 
		0.84620195627212524 1 1 1;
	setAttr -s 17 ".kiy[11:16]"  0.50669771432876587 0.075536973774433136 
		-0.53286236524581909 0 0 0;
	setAttr -s 17 ".kox[11:16]"  0.86212378740310669 0.9971429705619812 
		0.84620183706283569 1 1 1;
	setAttr -s 17 ".koy[11:16]"  0.50669771432876587 0.075536973774433136 
		-0.53286254405975342 0 0 0;
createNode animCurveTA -n "D_:R_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 -1.2474624697910552 2 -1.2474624697910552 
		5 -0.44810662283649344 7 -2.5618282019643299 9 -17.882829963354265 12 -20.42445100856143 
		14 -19.401747493952922 18 -14.082539441949024 20 -8.7754069064994376 23 -1.7903065925512605 
		26 -3.9275525476445279 28 0.9146320898306064 33 -1.2474624697910552 34 -1.2474624697910552 
		38 -1.2474624697910552 40 -1.2474624697910552 42 -1.2474624697910552 43 -1.2449011573583082 
		48 -1.8107996672670179 51 -1.2465081870088852 58 -1.2656089908749955 61 -1.2474624697910552;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 10;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 10;
createNode animCurveTA -n "D_:LShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 7.9063758384370013 5 18.263525417137032 
		7 19.332045690379626 9 19.332045690379626 12 19.332045690379626 14 19.071029958028955 
		18 18.263525417137032 20 16.263120248841513 23 8.5467462249149886 26 1.0494627398322265 
		30 -3.7938228206121649 35 -57.612708304499712 36 -60.8666036419581 39 -53.072325204025248 
		41 -47.906900535258913 43 -40.14283662599194 45 -31.280184455663925 50 -5.3314013489287007 
		57 1.0787374845503324 58 1.0787374845503324 61 0.16855300321728089 61.005 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:RElbowFK_rotateX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 3.1335258228302996 5 -17.623297024304318 
		7 -33.030420659838335 9 -42.26821178196699 12 -41.692577264049781 14 -41.083440204162649 
		18 -40.047906834742086 20 -33.170214266092152 23 -13.137114055524187 26 3.9693256046175693 
		30 2.3401711847434279 36 2.3213744747327882 37 2.3177204651565178 41 9.0231172296152344 
		43 7.2587149372195112 45 4.4684181065162836 47 2.2431421646132566 52 2.173937338773813 
		56 2.0756954076010405 57 2.0756954076010405 63 1.9073970430917353 65 1.8762307090162829;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:L_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1.7208864831813682 2 1.7208864831813682 
		5 -4.7621228486350438 7 -8.0533359868900636 9 -14.434147247183004 12 -14.431010293685162 
		14 -14.429848458796643 18 -14.427873338784996 20 -12.639828688529716 23 -6.2018458972714976 
		26 -0.26524461385708864 30 1.7208864831813682 35 1.7208864831813682 36 1.7208864831813682 
		39 1.7208864831813682 41 1.7208864831813682 43 1.7208864831813682 45 1.7208864831813682 
		50 1.7208864831813682 57 1.7208864831813682 58 1.7208864831813682 61 1.7208864831813682 
		61.005 1.7208864831813682;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:TankControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -0.70178127080170383 2 -0.70178127080170383 
		5 -0.70002474384102997 7 -0.70178127080170383 9 -0.70178127080170383 10 0.83073352186061122 
		14 0.83073352186061122 18 0.83073352186061122 20 0.83073352186061122 23 0.064476382437277435 
		26 -0.70178127080170383 28 -0.70178127080170383 33 -0.70178127080170383 34 -0.70178127080170383 
		38 -0.70178127080170383 40 -0.70178127080170383 42 -0.70178127080170383 43 -0.70178127080170383 
		48 -0.70178127080170383 51 -0.70178127080170383 52 -0.70178127080170383 58 -0.70178127080170383 
		61 -0.70178127080170383;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:R_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 7.555709087302473 5 0 7 0 9 0 12 
		0 14 0 18 0 20 0 23 2.5475818858538095 26 4.0761749303630248 28 -1.3584409189792828 
		32 -4.0961893557838778 33 -4.0844938533242754 36 0 40 0 42 0 43 0 48 0 51 0 52 0 
		58 -5.3632482469994471 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:L_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 25.46188064100717 2 1.5777860713541412 
		5 4.0258929669105132 7 5.1660827319703966 9 5.1660827319703966 12 5.1660827319703966 
		14 5.1660827319703966 18 5.1660827319703966 20 5.1660827319703966 23 5.1660827319703966 
		26 5.1660827319703966 28 5.1660827319703966 33 5.1660827319703966 34 5.1660827319703966 
		38 5.1660827319703966 40 5.1660827319703966 42 11.546992486330737 43 12.983138583643223 
		47 21.39546335581819 51 25.46188064100717 52 25.46188064100717 58 25.46188064100717 
		61 25.46188064100717;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTA -n "D_:RootControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -20.960601260446008 5 -11.195343610551431 
		7 -8.0257659113741067 9 -4.3403233262130305 12 -3.5632758007169198 14 -3.5325352726224883 
		18 -3.4802763563101347 20 -4.0447281943751188 23 -5.0642542114431723 26 -5.7900442977450446 
		28 -4.7879011361217847 33 4.2890165485744829 34 4.2936735961544992 38 -3.2047442481955324 
		40 -5.1351859144947216 42 -7.6492295332526448 43 -9.2508939016314855 48 -5.9930021852008251 
		51 -4.9305172052052271 52 -4.9305172052052271 58 -1.1968956876156269 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:Spine0Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -2.2165710503430822 5 -2.0753239100742613 
		7 -2.0753239100742613 9 -2.0753239100742613 12 -2.0753239100742613 14 -2.0753239100742613 
		18 -2.0753239100742613 20 -2.052393434467028 23 -1.9915296848051001 26 -1.9285688661879652 
		28 -1.8944542243435045 33 3.6110496702412118 34 3.3649832081622373 38 1.2450228721325669 
		40 0.99819640281599209 42 0.45080161510582001 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:Spine1Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.0084151252342212438 2 0.0084151252342212473 
		5 0.0084151252342212473 7 0.0084151252342212473 9 0.0084151252342212473 12 0.0084151252342212473 
		14 0.0084151252342212473 18 0.0084151252342212473 20 0.0084151252342212473 23 0.0084151252342212473 
		26 0.0084151252342212473 28 0.0084151252342212473 33 -1.4729181820759636 34 -1.4622401749736995 
		38 -1.3702449012846838 40 -1.3608234316424712 42 -1.3945353062227051 43 -1.3227218571551957 
		48 -0.3398115256866624 51 0.0042075648742228291 52 0.0042075648742228291 58 0.0077576928699502074 
		61 0.0084151252342212438;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:HeadControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -0.026536333767640648 2 -0.10669722103100428 
		5 -0.35151437909942174 7 -0.58127629509322032 9 -1.5104605141857885 12 -1.5104605141857885 
		14 -1.5104605141857885 18 -1.5104605141857885 20 -2.0490525546419334 23 -3.5119263721632552 
		26 -4.9574495731051149 28 -5.4035001471729833 33 -5.7838543063761758 34 -5.6736333504696397 
		38 -4.7240360231990151 40 -4.6267853489801434 42 -4.476031687997323 43 -4.2334917389167881 
		48 -3.0648927416856577 51 -2.6558825284577932 52 -2.6558825284577932 58 -0.43737233785596979 
		61 -0.026536333767640648;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:RShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -12.215538151373286 2 47.568613843535807 
		5 49.231697189657034 7 47.81405411360339 9 42.080938732504116 12 41.687810830328388 
		14 42.080938732504116 18 42.080938732504116 20 43.47747650987575 23 47.21219500168165 
		26 51.018780507682557 28 52.798597086886375 33 43.93010471580979 34 41.382625424830614 
		38 34.003679612421706 40 31.202276271311234 42 27.687842973327744 43 24.298074164387899 
		48 0.4872136799495716 51 -8.0909715422013946 52 -9.927922312289148 58 -14.046681236673718 
		61 -12.215538151373286;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 10 9 9 
		9 10 9 9 9 9 9 9 9 9 9 1 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 10 9 9 
		9 10 9 9 9 9 9 9 9 9 9 1 9 9 10;
	setAttr -s 23 ".kix[19:22]"  0.6514093279838562 0.91345810890197754 
		0.99125975370407104 1;
	setAttr -s 23 ".kiy[19:22]"  -0.75872647762298584 -0.4069330096244812 
		-0.13192488253116608 0;
	setAttr -s 23 ".kox[19:22]"  0.65140914916992188 0.91345810890197754 
		0.99125975370407104 1;
	setAttr -s 23 ".koy[19:22]"  -0.75872671604156494 -0.4069330096244812 
		-0.13192488253116608 0;
createNode animCurveTA -n "D_:RElbowFK_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 38.157091370305473 2 38.157091370305473 
		5 31.709862211276302 7 36.323167499802615 9 32.983785523040943 12 32.983785523040943 
		18 32.983785523040943 26 24.319046464056232 28 22.940176747316443 31 22.532996348392523 
		36 24.656183446006022 38 23.145102132693122 42 18.32331739299287 46 22.701694745322765 
		51 32.170454128825554 56 32.721384967107376 58 38.157091370305473;
	setAttr -s 17 ".kit[3:16]"  1 10 10 10 10 10 10 10 
		1 10 10 10 10 10;
	setAttr -s 17 ".kot[3:16]"  1 10 10 10 10 10 10 10 
		1 10 10 10 10 10;
	setAttr -s 17 ".kix[3:16]"  0.85409462451934814 1 1 1 1 1 1 1 0.9066283106803894 
		0.99957913160324097 0.77873945236206055 1 1 0.57494843006134033;
	setAttr -s 17 ".kiy[3:16]"  -0.52011770009994507 0 0 0 0 0 0 0 -0.42193028330802917 
		-0.029008733108639717 0.62734746932983398 0 0 0.81818962097167969;
	setAttr -s 17 ".kox[3:16]"  0.85409462451934814 1 1 1 1 1 1 1 0.9066283106803894 
		0.99957913160324097 0.77873945236206055 1 1 0.57494848966598511;
	setAttr -s 17 ".koy[3:16]"  -0.52011775970458984 0 0 0 0 0 0 0 -0.42193019390106201 
		-0.029008733108639717 0.62734746932983398 0 0 0.81818968057632446;
createNode animCurveTA -n "D_:R_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 9.6996556887373035 2 9.6996556887373035 
		5 9.5726838161211898 7 36.442439251214147 9 29.870900513928227 12 25.765819317524723 
		14 27.642421181779408 18 28.085740722977278 20 25.422823474203874 23 17.71422665848727 
		26 11.771254642947167 28 12.262049250245905 33 9.6996556887373035 34 9.6996556887373035 
		38 9.6996556887373035 40 9.6996556887373035 42 9.6996556887373035 43 8.9839248476765583 
		48 -5.9166661263304956 51 -9.4396210102361415 58 13.694093640005612 61 9.6996556887373035;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 10;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 10;
createNode animCurveTA -n "D_:LShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -35.394290393390072 5 -42.927741742989085 
		7 -43.538324756270576 9 -43.538324756270576 12 -43.538324756270576 14 -43.377992051557605 
		18 -42.927741742989085 20 -41.937990681965687 23 -39.178146626779203 26 -36.593334952439243 
		30 -35.186876641557873 35 -49.973892200096778 36 -52.398155595567196 39 -57.813166044044493 
		41 -57.55296292613918 43 -58.709059761282269 45 -56.500669721874061 50 -25.303854837923907 
		57 -6.5467134541325231 58 -5.4229298046097147 61 -0.83799410211372527 61.005 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 1 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 1 9 9 10;
	setAttr -s 23 ".kix[19:22]"  0.78308224678039551 0.80104857683181763 
		0.72684609889984131 1;
	setAttr -s 23 ".kiy[19:22]"  0.62191826105117798 0.59859931468963623 
		0.68680042028427124 0;
	setAttr -s 23 ".kox[19:22]"  0.78308218717575073 0.80104857683181763 
		0.72684609889984131 1;
	setAttr -s 23 ".koy[19:22]"  0.62191826105117798 0.59859931468963623 
		0.68680042028427124 0;
createNode animCurveTA -n "D_:RElbowFK_rotateY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 27.786332416329678 2 37.36568026950529 
		5 36.951027874730691 7 30.742071609232283 9 26.151025151470211 12 25.802353225897566 
		14 25.433388222935307 18 24.80614749523015 20 25.136546615826894 23 25.491577946968327 
		26 26.920701867049281 30 34.940354780309228 36 34.882892281556131 37 34.871721788389962 
		41 25.744143046319596 43 28.031180526977206 45 31.723478524779619 47 34.643732129585331 
		52 34.432169456316636 56 34.131838874830734 57 34.131838874830734 63 33.61734221263346 
		65 33.522065143830694;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:L_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -8.1070161891185535 2 -8.1070161891185535 
		5 -12.521912915499232 7 -15.804910016671904 9 -22.785723096670473 12 -23.196361008950184 
		14 -23.348449158603675 18 -23.606999104799371 20 -22.133583324769027 23 -16.482682646144344 
		26 -11.077141688034171 30 -8.1070161891185535 35 -8.1070161891185535 36 -8.1070161891185535 
		39 -8.1070161891185535 41 -8.1070161891185535 43 -8.1070161891185535 45 -8.1070161891185535 
		50 -8.1070161891185535 57 -8.1070161891185535 58 -8.1070161891185535 61 -8.1070161891185535 
		61.005 -8.1070161891185535;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:TankControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0.049622903830317373 7 0 9 0 
		10 0.5642587631903877 14 0.5642587631903877 18 0.5642587631903877 20 0.5642587631903877 
		23 0.28212947618644746 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 
		0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:R_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -29.070586091195047 2 -29.070586091195047 
		5 -29.070586091195047 7 -29.070586091195047 9 -29.070586091195047 12 -29.070586091195047 
		14 -29.070586091195047 18 -29.070586091195047 20 -29.070586091195047 23 -29.335530594445668 
		26 -29.070586091195047 28 -26.244510484651006 32 -23.766930272819266 33 -23.135662957237688 
		36 -20.69947051020123 40 -20.69947051020123 42 -20.69947051020123 43 -20.69947051020123 
		48 -20.69947051020123 51 -20.69947051020123 52 -20.69947051020123 58 -26.585408634496613 
		61 -29.070586091195047;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:L_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -2.2309053162694341 5 0 7 0 9 0 12 
		0 14 0 18 0 20 0 23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 47 0 51 0 52 0 58 0 
		61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTA -n "D_:RootControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -6.5528985837000073 5 11.967220754279293 
		7 18.379739281856438 9 22.120389957014709 12 23.732872359106871 14 23.58160696912914 
		18 23.32445571487877 20 22.910965892282551 23 21.422239059934309 26 17.853661196529075 
		28 6.3476383593528594 33 1.5113420896085801 34 1.0466620548350367 38 0.42454774000103751 
		40 0.30026249995479171 42 -0.4700243893096484 43 -1.0451994860316411 48 -0.14835327670013768 
		51 0.16554332956122264 52 0.16554332956122264 58 0.025866186871000691 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:Spine0Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0.11411562144956751 5 0.10684380167764712 
		7 0.10684380167764712 9 0.10684380167764712 12 0.10684380167764712 14 0.10684380167764712 
		18 0.10684380167764712 20 0.10566327309786233 23 0.10252982759482204 26 0.099288418767024378 
		28 0.097532096291366555 33 3.5019766225359716 34 3.2524596548359122 38 1.1027718419524468 
		40 0.88414671762471009 42 0.39929493677042888 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:Spine1Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0.019794606311957463 2 0.01979460631195747 
		5 0.01979460631195747 7 0.01979460631195747 9 0.01979460631195747 12 0.01979460631195747 
		14 0.01979460631195747 18 0.01979460631195747 20 0.01979460631195747 23 0.01979460631195747 
		26 0.01979460631195747 28 0.01979460631195747 33 0.074605328199109935 34 -0.066546499017893668 
		38 -1.2826255718422184 40 -1.4071673184744953 42 -1.7624097101370417 43 -1.9108293602035498 
		48 -0.48806937371204467 51 0.009897308465305248 52 0.009897308465305248 58 0.018248151034671402 
		61 0.019794606311957463;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:HeadControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -0.034361262366484416 2 0.064756055820499564 
		5 0.36746752744952565 7 0.65156349648629042 9 1.8004810183261477 12 1.8004810183261477 
		14 1.8004810183261477 18 1.8004810183261477 20 2.2603255491055165 23 3.9412639818759265 
		26 5.7062190012175398 28 6.6141459037359942 33 7.084446137055715 34 6.92360110445264 
		38 5.7740022287932389 40 5.5451228658286515 42 5.4122257892176258 43 5.1674541259019398 
		48 3.6284301408477826 51 3.2167721432354526 52 3.2167721432354526 58 0.93081992647850897 
		61 -0.034361262366484416;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:RShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 47.399331424850153 2 27.945953150658145 
		5 5.5687973045174521 7 23.428791503865551 9 34.318703400274956 12 35.065440213885445 
		14 34.318703400274956 18 34.318703400274956 20 32.566199377356796 23 26.157272772570909 
		26 19.411733549758811 28 15.863982904378222 33 21.283139553456028 34 26.063046564031239 
		38 34.44284251915429 40 37.860789194478606 42 40.723455327962832 43 42.459213171960315 
		48 50.517864410507315 51 53.751924135544165 52 53.751924135544165 58 49.569506864230647 
		61 47.399331424850153;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 10 9 9 
		9 10 9 9 9 9 9 9 1 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 10 9 9 
		9 10 9 9 9 9 9 9 1 9 9 10 9 9 10;
	setAttr -s 23 ".kix[16:22]"  0.75869423151016235 0.76016473770141602 
		0.80418550968170166 1 0.95438641309738159 0.93799036741256714 1;
	setAttr -s 23 ".kiy[16:22]"  0.65144693851470947 0.64973044395446777 
		0.59437829256057739 0 -0.29857426881790161 -0.34666147828102112 0;
	setAttr -s 23 ".kox[16:22]"  0.75869417190551758 0.76016473770141602 
		0.80418550968170166 1 0.95438641309738159 0.93799036741256714 1;
	setAttr -s 23 ".koy[16:22]"  0.65144699811935425 0.64973044395446777 
		0.59437829256057739 0 -0.29857426881790161 -0.34666147828102112 0;
createNode animCurveTA -n "D_:RElbowFK_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 11.846871456790783 7 -4.5502449740323705 
		9 -19.212338280134066 12 -20.505637825176585 18 -20.505637825176585 26 -1.6993801012281309 
		28 6.6749655695465142 31 12.064282993173963 36 11.314947202308133 38 12.518118043013436 
		42 15.09603730173551 46 7.8670994966723615 51 2.6266510855155056 56 0.76978595805615468 
		58 0;
	setAttr -s 17 ".kit[4:16]"  1 10 10 10 10 10 10 1 
		10 10 1 10 10;
	setAttr -s 17 ".kot[4:16]"  1 10 10 10 10 10 10 1 
		10 10 1 10 10;
	setAttr -s 17 ".kix[4:16]"  0.84294575452804565 1 1 0.57491946220397949 
		0.57004112005233765 1 1 0.88707321882247925 1 0.80944138765335083 0.97673100233078003 
		1 1;
	setAttr -s 17 ".kiy[4:16]"  -0.53799855709075928 0 0 0.8182099461555481 
		0.82161617279052734 0 0 0.46162879467010498 0 -0.58720064163208008 -0.21446841955184937 
		0 0;
	setAttr -s 17 ".kox[4:16]"  0.84294503927230835 1 1 0.57491946220397949 
		0.57004112005233765 1 1 0.88707315921783447 1 0.80944138765335083 0.97673094272613525 
		1 1;
	setAttr -s 17 ".koy[4:16]"  -0.53799974918365479 0 0 0.8182099461555481 
		0.82161617279052734 0 0 0.46162885427474976 0 -0.58720064163208008 -0.21446838974952698 
		0 0;
createNode animCurveTA -n "D_:R_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 7.1301771055535337 2 7.1301771055535337 
		5 -47.277985351148658 7 -82.202730337197309 9 -97.937362559874742 12 -102.45304041754254 
		14 -102.22161057390323 18 -99.421782568510423 20 -88.689245124830038 23 -87.152046475960532 
		26 -88.687452092587165 28 -35.884077874484362 33 7.1301771055535337 34 7.1301771055535337 
		38 7.1301771055535337 40 7.1301771055535337 42 7.1301771055535337 43 7.1459558521487825 
		48 7.5941244864001867 51 7.5448550669511398 58 7.0407224282698717 61 7.1301771055535337;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 10;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 10;
createNode animCurveTA -n "D_:LShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -45.645013483740549 2 -33.345578021061975 
		5 -38.958637638388275 7 -39.569220651669767 9 -39.569220651669767 12 -39.569220651669767 
		14 -39.408779076297236 18 -38.958637638388275 20 -37.970379557298692 23 -34.123832152802372 
		26 -30.296974057926221 30 -27.274578298397294 35 -32.579033646671093 36 -31.688677117488474 
		39 -23.678624863544272 41 -19.84789990868947 43 -16.41269687508154 45 -16.141426720337471 
		50 -37.017482015377965 57 -43.05809876531093 58 -43.539720329392132 61 -45.240807408487626 
		61.005 -45.645013483740549;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:RElbowFK_rotateZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 7.9060931908893721 5 -15.758163754182325 
		7 -30.776304578607753 9 -50.360518109048414 12 -50.839682919666565 14 -51.346735632553553 
		18 -52.208725550466269 20 -44.050900482568402 23 -13.975800633037181 26 11.623981706761581 
		30 5.9044068934795924 36 5.8569815491778145 37 5.8477622410042818 41 17.804512055133195 
		43 14.639101094136814 45 9.6458339194263036 47 5.6595962492587235 52 5.4849878900872291 
		56 5.2371169909722726 57 5.2371169909722726 63 4.812489070566941 65 4.7338543454838842;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:L_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -1.1231193682533216 2 -1.1231193682533216 
		5 -10.239674607252756 7 -12.925392457206597 9 -16.983774477312618 12 -16.786972825193942 
		14 -16.714083308117175 18 -16.59017108509806 20 -15.577657182545574 23 -11.483740660722559 
		26 -7.0166718533308883 30 -1.1231193682533216 35 -1.1231193682533216 36 -1.1231193682533216 
		39 -1.1231193682533216 41 -1.1231193682533216 43 -1.1231193682533216 45 -1.1231193682533216 
		50 -1.1231193682533216 57 -1.1231193682533216 58 -1.1231193682533216 61 -1.1231193682533216 
		61.005 -1.1231193682533216;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:TankControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 1.342545913462966 7 0 9 0 10 
		-2.5157638653215879 14 -2.5157638653215879 18 -2.5157638653215879 20 -2.5157638653215879 
		23 -1.257882354398602 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 
		61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:R_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 32 0 33 0 36 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:L_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 47 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTU -n "D_:HipControl_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RootControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:Spine0Control_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine1Control_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RShoulderFK_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 17.995 1 18 1;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleX1";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:L_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:R_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 32 1 33 1 36 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:L_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 47 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTU -n "D_:HipControl_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1.0000000000000002;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RootControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:Spine0Control_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine1Control_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RShoulderFK_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 17.995 1 18 1;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleY1";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:L_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:R_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 32 1 33 1 36 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:L_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 47 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTU -n "D_:HipControl_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1.0000000000000002;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RootControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:Spine0Control_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine1Control_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RShoulderFK_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 17.995 1 18 1;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleZ1";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:L_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:R_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 32 1 33 1 36 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:L_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 47 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 5 9 5 9 9 5;
createNode animCurveTU -n "D_:RootControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:Spine0Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:Spine1Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:HeadControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:R_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:RShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 5 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:RElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 28 1 
		31 1 36 1 38 1 42 1 46 1 51 1 56 1 58 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:R_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 22 ".ktv[0:21]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 58 1 61 1;
	setAttr -s 22 ".kit[0:21]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 22 ".kot[0:21]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 5;
createNode animCurveTU -n "D_:L_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:LShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 30 1 35 1 36 1 39 1 41 1 43 1 45 1 50 1 57 1 58 1 61 1 61.005 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:RElbowFK_visibility1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 30 1 36 1 37 1 41 1 43 1 45 1 47 1 52 1 56 1 57 1 63 1 65 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:L_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 30 1 35 1 36 1 39 1 41 1 43 1 45 1 50 1 57 1 58 1 61 1 61.005 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:TankControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 10 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:R_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:L_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:R_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 32 1 33 1 36 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 5 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:L_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 47 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTU -n "D_:R_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 32 0 33 0 36 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:L_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 47 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 10 9 10 9 9 10;
createNode animCurveTU -n "D_:R_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 2.6 5 16.8 7 19.452234991767991 9 
		19.452234991767991 12 19.452234991767991 14 19.452234991767991 18 19.452234991767991 
		20 19.941723526073606 23 21.517304537609959 26 21.900000000000002 28 16.02742710255248 
		32 7.2574861981176237 33 5.2336898426245364 36 0 40 0 42 0 43 0 48 0 51 0 52 0 58 
		0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 10 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:r_thumb_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 4.6514645037620239 15 0 35 5.5128474764561419 
		55 0 75 4.6514645037620239;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 15 0 35 0 55 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 11 0 31 0 51 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_2_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 20 0 40 0 54 0 75 0;
	setAttr -s 5 ".kit[1:4]"  3 9 10 10;
	setAttr -s 5 ".kot[1:4]"  3 9 10 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_thumb_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 11.870872562983752 15 0 35 14.069183974047057 
		55 0 75 11.870872562983752;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 15 0 35 0 55 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 11 0 31 0 51 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_2_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 20 0 40 0 54 0 75 0;
	setAttr -s 5 ".kit[1:4]"  3 9 10 10;
	setAttr -s 5 ".kot[1:4]"  3 9 10 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_thumb_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -38.285706776879671 15 0 35 -45.375657885501766 
		55 0 75 -38.285706776879671;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -67.298744458395092 15 -37.069649276763101 
		35 -72.896729318506942 55 -37.069649276763101 75 -67.298744458395092;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -56.936986476047892 11 -34.609429738102065 
		31 -73.456858763929191 51 -34.609429738102065 75 -56.936986476047892;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_2_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -74.810673852666966 20 -42.812164971849889 
		40 -74.810673852666966 54 -74.810673852666966 75 -74.810673852666966;
	setAttr -s 5 ".kit[1:4]"  3 9 10 10;
	setAttr -s 5 ".kot[1:4]"  3 9 10 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 16 0 36 0 56 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 16 0 36 0 56 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 12 0 32 0 52 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 20 0 40 0 60 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 16 0 36 0 56 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 16 0 36 0 56 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 12 0 32 0 52 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 20 0 40 0 60 0 75 0;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -39.542557426213627 16 0 36 -44.132318556041994 
		56 0 75 -39.542557426213627;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -65.326320103044935 16 -37.526539377836308 
		36 -68.553080365792368 56 -37.526539377836308 75 -65.326320103044935;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -58.159773063591629 12 -42.0780670191971 
		32 -66.895514618571369 52 -42.0780670191971 75 -58.159773063591629;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -73.966144308812432 20 -29.931065482458106 
		40 -73.966144308812432 60 -29.931065482458106 75 -73.966144308812432;
	setAttr -s 5 ".kit[0:4]"  10 3 9 3 10;
	setAttr -s 5 ".kot[0:4]"  10 3 9 3 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:LElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 -26.771605834649325 5 -31.489607389857092 
		7 -30.771254815105312 9 -28.04203795269704 12 -22.710908259027672 14 -21.457425323520621 
		18 -19.011474309012097 20 -18.430476675444723 23 -26.391500072553491 26 -30.131970708807653 
		30 -4.1940428463792419 35 39.678757202508301 36 33.869617423902319 39 -23.216063910267369 
		41 -51.522176330610272 43 -78.061130267286103 45 -93.785625593525097 50 -60.046455428901233 
		57 -6.0351427859604145 58 -1.8610892305899693 61 0.27968933035494831 61.005 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 1 1 1 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 1 1 1 10;
	setAttr -s 23 ".kix[19:22]"  0.4017428457736969 0.82291305065155029 
		0.98986297845840454 1;
	setAttr -s 23 ".kiy[19:22]"  0.91575253009796143 0.56816744804382324 
		-0.14202584326267242 0;
	setAttr -s 23 ".kox[19:22]"  0.40174290537834167 0.82291251420974731 
		0.98986297845840454 1;
	setAttr -s 23 ".koy[19:22]"  0.91575253009796143 0.56816816329956055 
		-0.14202587306499481 0;
createNode animCurveTA -n "D_:LElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 -32.439136911498778 2 -39.357026181608866 
		5 -30.456479442795846 7 -31.18370371427525 9 -33.58861961836957 12 -37.169256632344734 
		14 -38.299811124029823 18 -39.713529639848787 20 -39.713529639848787 23 -33.697805773249542 
		26 -31.797248684560468 30 -57.252650663674387 35 -73.428600420882063 36 -76.154744534252472 
		39 -82.427780370453405 41 -82.352980591635159 43 -83.66904976267962 45 -82.050481145953583 
		50 -55.375307702952497 57 -36.07670285426326 58 -34.310757119298835 61 -32.482278052211754 
		61.005 -32.439136911498778;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 1 9 1 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 1 9 1 10;
	setAttr -s 23 ".kix[19:22]"  0.71618914604187012 0.90484625101089478 
		0.99434500932693481 1;
	setAttr -s 23 ".kiy[19:22]"  0.6979062557220459 0.42573842406272888 
		-0.1061982586979866 0;
	setAttr -s 23 ".kox[19:22]"  0.71618920564651489 0.90484625101089478 
		0.99434500932693481 1;
	setAttr -s 23 ".koy[19:22]"  0.6979062557220459 0.42573842406272888 
		-0.10619818419218063 0;
createNode animCurveTA -n "D_:LElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 1.9823422924459311 5 48.483343891823772 
		7 45.887086197032872 9 40.426908551309118 12 24.6688216482518 14 23.748865422703727 
		18 21.479787768081707 20 21.13238294808156 23 27.059281255959544 26 25.716287967073239 
		30 -22.03795972037442 35 -68.677963025622915 36 -65.256973101698833 39 -21.06923691700063 
		41 1.2870629212134672 43 23.677277277601469 45 38.616396249145055 50 23.755696721227949 
		57 0.49227931953550547 58 0.49227931953550547 61 -0.83627283987985179 61.005 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 1 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 1 10;
	setAttr -s 23 ".kix[21:22]"  0.96859800815582275 1;
	setAttr -s 23 ".kiy[21:22]"  0.24863213300704956 0;
	setAttr -s 23 ".kox[21:22]"  0.96859800815582275 1;
	setAttr -s 23 ".koy[21:22]"  0.24863214790821075 0;
createNode animCurveTU -n "D_:Entity_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTL -n "D_:Entity_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:Entity_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:Entity_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:Entity_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:Entity_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:Entity_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:Entity_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:Entity_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:Entity_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:DiverGlobal_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTL -n "D_:DiverGlobal_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:DiverGlobal_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTL -n "D_:DiverGlobal_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:DiverGlobal_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:DiverGlobal_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:DiverGlobal_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 0 26 0 28 0 33 0 34 0 38 0 40 0 42 0 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:DiverGlobal_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:DiverGlobal_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:DiverGlobal_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:HeadControl_Mask";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 49.900000000000006 7 97.726772956813818 
		9 88.341035713057522 12 72.687266361362049 14 73.112715203223956 18 79.066470765578742 
		20 79.588426965345704 23 78.931485665407493 26 77.394983157880006 28 73.225949177033371 
		33 79.5 34 79.609060286975222 38 73.7 40 49.813426280211388 42 21.532456992175995 
		43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:LElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 30 1 35 1 36 1 39 1 41 1 43 1 45 1 50 1 57 1 58 1 61 1 61.005 1;
	setAttr -s 23 ".kit[0:22]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 23 ".kot[0:22]"  3 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTU -n "D_:r_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 54 1 75 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTU -n "D_:r_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 75 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:r_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 75 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:r_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 75 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:l_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 75 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:l_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 75 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:l_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 75 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:l_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 75 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode polyPlane -n "polyPlane1";
	setAttr ".cuv" 2;
createNode displayLayer -n "layer1";
	setAttr ".dt" 2;
	setAttr ".do" 1;
createNode animCurveTU -n "D_:HipControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 1 2 1 5 1 7 1 9 1 12 1 14 1 18 1 20 1 
		23 1 26 1 28 1 33 1 34 1 38 1 40 1 42 1 43 1 48 1 51 1 52 1 58 1 61 1;
	setAttr -s 23 ".kot[0:22]"  5 9 9 9 9 9 9 9 
		9 5 9 9 9 9 9 9 5 9 9 5 9 9 5;
createNode animCurveTA -n "D_:HipControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 3.3686024279490079 26 5.673437557455288 28 0 33 0 34 0 38 0.015963258588553679 
		40 0 42 -0.0013302724330203875 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  10 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  10 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTA -n "D_:HipControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 7.2798116407249278 2 7.2798116407249278 
		5 7.2798116407249278 7 7.2798116407249278 9 7.2798116407249278 12 1.5481741302721881 
		14 -0.43180559085673853 18 -1.8650376549038683 20 -1.9752360292063562 23 -6.0328398711337847 
		26 -10.852167097460077 28 -15.024876703013707 33 -17.288202634907488 34 -14.790649412435949 
		38 15.081895804585843 40 20.853188754968475 42 21.662063772857628 43 18.403802528563521 
		48 -11.565455837292236 51 -21.788985859495174 52 -19.252242219477765 58 5.3911283782895252 
		61 7.2798116407249278;
	setAttr -s 23 ".kit[0:22]"  10 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 1 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  10 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 1 9 9 10 9 9 10;
	setAttr -s 23 ".kix[16:22]"  0.94958221912384033 0.32602611184120178 
		0.35533195734024048 1 0.44136592745780945 0.54371792078018188 1;
	setAttr -s 23 ".kiy[16:22]"  -0.31351825594902039 -0.94536077976226807 
		-0.93474012613296509 0 0.8973272442817688 0.8392680287361145 0;
	setAttr -s 23 ".kox[16:22]"  0.94958221912384033 0.32602611184120178 
		0.35533195734024048 1 0.44136592745780945 0.54371792078018188 1;
	setAttr -s 23 ".koy[16:22]"  -0.31351810693740845 -0.94536077976226807 
		-0.93474012613296509 0 0.8973272442817688 0.8392680287361145 0;
createNode animCurveTA -n "D_:HipControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 23 ".ktv[0:22]"  0 0 2 0 5 0 7 0 9 0 12 0 14 0 18 0 20 0 
		23 -6.879564693227052 26 -11.586638988801818 28 0 33 0 34 0 38 -0.88405211087192936 
		40 0 42 0.073671049473757924 43 0 48 0 51 0 52 0 58 0 61 0;
	setAttr -s 23 ".kit[0:22]"  10 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
	setAttr -s 23 ".kot[0:22]"  10 9 9 9 9 9 9 9 
		9 10 9 9 9 9 9 9 10 9 9 10 9 9 10;
createNode animCurveTU -n "D_:r_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 54 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTA -n "D_:r_mid_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 9 2.8956420155903957 54 0;
createNode animCurveTA -n "D_:r_mid_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 9 -1.1220664252250794 54 0;
createNode animCurveTA -n "D_:r_mid_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 -53.468796 9 -34.816102078286875 54 -53.468796;
createNode animCurveTU -n "D_:r_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 54 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTA -n "D_:r_pink_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 9 1.8293600175074181 54 0;
createNode animCurveTA -n "D_:r_pink_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 9 -2.5096386522822844 54 0;
createNode animCurveTA -n "D_:r_pink_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 -80.954255 9 -29.572885658612808 54 -80.954255;
createNode animCurveTU -n "D_:r_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 54 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTA -n "D_:r_thumb_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 -12.490945 9 3.0395606673732063 54 -12.490945;
createNode animCurveTA -n "D_:r_thumb_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 -16.026247 9 -47.045137672893155 54 -16.026247;
createNode animCurveTA -n "D_:r_thumb_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 -7.016297 9 -25.08823800071605 54 -7.016297;
createNode animCurveTU -n "D_:l_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 1;
	setAttr ".kot[0]"  5;
createNode animCurveTA -n "D_:l_thumb_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 -18.444187;
createNode animCurveTA -n "D_:l_thumb_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 -17.180223;
createNode animCurveTA -n "D_:l_thumb_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 -9.279406;
createNode animCurveTU -n "D_:l_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 1;
	setAttr ".kot[0]"  5;
createNode animCurveTA -n "D_:l_point_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 -3.9509770000000004;
createNode animCurveTA -n "D_:l_point_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 -1.9157829999999998;
createNode animCurveTA -n "D_:l_point_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 -38.738955;
createNode animCurveTU -n "D_:l_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 1;
	setAttr ".kot[0]"  5;
createNode animCurveTA -n "D_:l_mid_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 0;
createNode animCurveTA -n "D_:l_mid_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 0;
createNode animCurveTA -n "D_:l_mid_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 -61.397553999999992;
createNode animCurveTU -n "D_:l_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 1;
	setAttr ".kot[0]"  5;
createNode animCurveTA -n "D_:l_pink_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 0;
createNode animCurveTA -n "D_:l_pink_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 0;
createNode animCurveTA -n "D_:l_pink_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  54 -69.87654400000001;
createNode animCurveTL -n "D_:L_RL_Base_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 8.3628139623411037 2 8.3628139623411037 
		5 8.3628139623411037 7 8.3628139623411037 9 8.3628139623411037 12 8.3628139623411037 
		18 8.3628139623411037 26 8.3628139623411037 30 8.3628139623411037 34 8.3628139623411037 
		39 8.3628139623411037 41 8.3628139623411037 45 8.3628139623411037 50 8.3628139623411037 
		55 8.3628139623411037 61 8.3628139623411037 63 8.3628139623411037;
createNode animCurveTL -n "D_:L_RL_Base_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.018230697272605966 2 0.018230697272605966 
		5 0.018230697272605966 7 0.018230697272605966 9 0.018230697272605966 12 0.018230697272605966 
		18 0.018230697272605966 26 0.018230697272605966 30 0.018230697272605966 34 0.018230697272605966 
		39 0.018230697272605966 41 0.018230697272605966 45 0.018230697272605966 50 0.018230697272605966 
		55 0.018230697272605966 61 0.018230697272605966 63 0.018230697272605966;
createNode animCurveTL -n "D_:L_RL_Base_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -11.660800466571922 2 -11.660800466571922 
		5 -11.660800466571922 7 -11.660800466571922 9 -11.660800466571922 12 -11.660800466571922 
		18 -11.660800466571922 26 -11.660800466571922 30 -11.660800466571922 34 -11.660800466571922 
		39 -11.660800466571922 41 -11.660800466571922 45 -11.660800466571922 50 -11.660800466571922 
		55 -11.660800466571922 61 -11.660800466571922 63 -11.660800466571922;
createNode animCurveTL -n "D_:L_RL_Toe_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:L_RL_Toe_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -3.4694469519536142e-018 2 -3.4694469519536142e-018 
		5 -3.4694469519536142e-018 7 -3.4694469519536142e-018 9 -3.4694469519536142e-018 
		12 -3.4694469519536142e-018 18 -3.4694469519536142e-018 26 -3.4694469519536142e-018 
		30 -3.4694469519536142e-018 34 -3.4694469519536142e-018 39 -3.4694469519536142e-018 
		41 -3.4694469519536142e-018 45 -3.4694469519536142e-018 50 -3.4694469519536142e-018 
		55 -3.4694469519536142e-018 61 -3.4694469519536142e-018 63 -3.4694469519536142e-018;
createNode animCurveTL -n "D_:L_RL_Toe_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 24.692967641450995 2 24.692967641450995 
		5 24.692967641450995 7 24.692967641450995 9 24.692967641450995 12 24.692967641450995 
		18 24.692967641450995 26 24.692967641450995 30 24.692967641450995 34 24.692967641450995 
		39 24.692967641450995 41 24.692967641450995 45 24.692967641450995 50 24.692967641450995 
		55 24.692967641450995 61 24.692967641450995 63 24.692967641450995;
createNode animCurveTL -n "D_:L_RL_Ball_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:L_RL_Ball_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 3.4694469519536142e-018 2 3.4694469519536142e-018 
		5 3.4694469519536142e-018 7 3.4694469519536142e-018 9 3.4694469519536142e-018 12 
		3.4694469519536142e-018 18 3.4694469519536142e-018 26 3.4694469519536142e-018 30 
		3.4694469519536142e-018 34 3.4694469519536142e-018 39 3.4694469519536142e-018 41 
		3.4694469519536142e-018 45 3.4694469519536142e-018 50 3.4694469519536142e-018 55 
		3.4694469519536142e-018 61 3.4694469519536142e-018 63 3.4694469519536142e-018;
createNode animCurveTL -n "D_:L_RL_Ball_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -11.97981014596937 2 -11.97981014596937 
		5 -11.97981014596937 7 -11.97981014596937 9 -11.97981014596937 12 -11.97981014596937 
		18 -11.97981014596937 26 -11.97981014596937 30 -11.97981014596937 34 -11.97981014596937 
		39 -11.97981014596937 41 -11.97981014596937 45 -11.97981014596937 50 -11.97981014596937 
		55 -11.97981014596937 61 -11.97981014596937 63 -11.97981014596937;
createNode animCurveTL -n "D_:L_RL_Ankle_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:L_RL_Ankle_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 13.613420620419738 2 13.613420620419738 
		5 13.613420620419738 7 13.613420620419738 9 13.613420620419738 12 13.613420620419738 
		18 13.613420620419738 26 13.613420620419738 30 13.613420620419738 34 13.613420620419738 
		39 13.613420620419738 41 13.613420620419738 45 13.613420620419738 50 13.613420620419738 
		55 13.613420620419738 61 13.613420620419738 63 13.613420620419738;
createNode animCurveTL -n "D_:L_RL_Ankle_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -6.1714173479236107 2 -6.1714173479236107 
		5 -6.1714173479236107 7 -6.1714173479236107 9 -6.1714173479236107 12 -6.1714173479236107 
		18 -6.1714173479236107 26 -6.1714173479236107 30 -6.1714173479236107 34 -6.1714173479236107 
		39 -6.1714173479236107 41 -6.1714173479236107 45 -6.1714173479236107 50 -6.1714173479236107 
		55 -6.1714173479236107 61 -6.1714173479236107 63 -6.1714173479236107;
createNode animCurveTL -n "D_:ikHandle_l_ankle_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:ikHandle_l_ankle_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 2.9101837384359897e-008 2 2.9101837384359897e-008 
		5 2.9101837384359897e-008 7 2.9101837384359897e-008 9 2.9101837384359897e-008 12 
		2.9101837384359897e-008 18 2.9101837384359897e-008 26 2.9101837384359897e-008 30 
		2.9101837384359897e-008 34 2.9101837384359897e-008 39 2.9101837384359897e-008 41 
		2.9101837384359897e-008 45 2.9101837384359897e-008 50 2.9101837384359897e-008 55 
		2.9101837384359897e-008 61 2.9101837384359897e-008 63 2.9101837384359897e-008;
createNode animCurveTL -n "D_:ikHandle_l_ankle_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 2.7778446209936192e-009 2 2.7778446209936192e-009 
		5 2.7778446209936192e-009 7 2.7778446209936192e-009 9 2.7778446209936192e-009 12 
		2.7778446209936192e-009 18 2.7778446209936192e-009 26 2.7778446209936192e-009 30 
		2.7778446209936192e-009 34 2.7778446209936192e-009 39 2.7778446209936192e-009 41 
		2.7778446209936192e-009 45 2.7778446209936192e-009 50 2.7778446209936192e-009 55 
		2.7778446209936192e-009 61 2.7778446209936192e-009 63 2.7778446209936192e-009;
createNode animCurveTL -n "D_:ikHandle_l_ball_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:ikHandle_l_ball_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1.8935795920854703e-008 2 1.8935795920854703e-008 
		5 1.8935795920854703e-008 7 1.8935795920854703e-008 9 1.8935795920854703e-008 12 
		1.8935795920854703e-008 18 1.8935795920854703e-008 26 1.8935795920854703e-008 30 
		1.8935795920854703e-008 34 1.8935795920854703e-008 39 1.8935795920854703e-008 41 
		1.8935795920854703e-008 45 1.8935795920854703e-008 50 1.8935795920854703e-008 55 
		1.8935795920854703e-008 61 1.8935795920854703e-008 63 1.8935795920854703e-008;
createNode animCurveTL -n "D_:ikHandle_l_ball_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -5.0356948833041315e-008 2 -5.0356948833041315e-008 
		5 -5.0356948833041315e-008 7 -5.0356948833041315e-008 9 -5.0356948833041315e-008 
		12 -5.0356948833041315e-008 18 -5.0356948833041315e-008 26 -5.0356948833041315e-008 
		30 -5.0356948833041315e-008 34 -5.0356948833041315e-008 39 -5.0356948833041315e-008 
		41 -5.0356948833041315e-008 45 -5.0356948833041315e-008 50 -5.0356948833041315e-008 
		55 -5.0356948833041315e-008 61 -5.0356948833041315e-008 63 -5.0356948833041315e-008;
createNode animCurveTL -n "D_:ikHandle_l_toe_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode pairBlend -n "pairBlend1";
	setAttr ".txm" 2;
	setAttr ".tzm" 2;
	setAttr ".rm" 2;
createNode animCurveTL -n "pairBlend1_inTranslateY1";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:ikHandle_l_toe_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -5.0355204450625024e-008 2 -5.0355204450625024e-008 
		5 -5.0355204450625024e-008 7 -5.0355204450625024e-008 9 -5.0355204450625024e-008 
		12 -5.0355204450625024e-008 18 -5.0355204450625024e-008 26 -5.0355204450625024e-008 
		30 -5.0355204450625024e-008 34 -5.0355204450625024e-008 39 -5.0355204450625024e-008 
		41 -5.0355204450625024e-008 45 -5.0355204450625024e-008 50 -5.0355204450625024e-008 
		55 -5.0355204450625024e-008 61 -5.0355204450625024e-008 63 -5.0355204450625024e-008;
createNode animCurveTL -n "D_:R_RL_Base_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -8.3628098299999998 2 -8.3628098299999998 
		5 -8.3628098299999998 7 -8.3628098299999998 9 -8.3628098299999998 12 -8.3628098299999998 
		18 -8.3628098299999998 26 -8.3628098299999998 30 -8.3628098299999998 34 -8.3628098299999998 
		39 -8.3628098299999998 41 -8.3628098299999998 45 -8.3628098299999998 50 -8.3628098299999998 
		55 -8.3628098299999998 61 -8.3628098299999998 63 -8.3628098299999998;
createNode animCurveTL -n "D_:R_RL_Base_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.01823068253 2 0.01823068253 5 0.01823068253 
		7 0.01823068253 9 0.01823068253 12 0.01823068253 18 0.01823068253 26 0.01823068253 
		30 0.01823068253 34 0.01823068253 39 0.01823068253 41 0.01823068253 45 0.01823068253 
		50 0.01823068253 55 0.01823068253 61 0.01823068253 63 0.01823068253;
createNode animCurveTL -n "D_:R_RL_Base_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -11.66079504 2 -11.66079504 5 -11.66079504 
		7 -11.66079504 9 -11.66079504 12 -11.66079504 18 -11.66079504 26 -11.66079504 30 
		-11.66079504 34 -11.66079504 39 -11.66079504 41 -11.66079504 45 -11.66079504 50 -11.66079504 
		55 -11.66079504 61 -11.66079504 63 -11.66079504;
createNode animCurveTL -n "D_:R_RL_Toe_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1.4913067861499485e-005 2 -1.4913067861499485e-005 
		5 -1.4913067861499485e-005 7 -1.4913067861499485e-005 9 -1.4913067861499485e-005 
		12 -1.4913067861499485e-005 18 -1.4913067861499485e-005 26 -1.4913067861499485e-005 
		30 -1.4913067861499485e-005 34 -1.4913067861499485e-005 39 -1.4913067861499485e-005 
		41 -1.4913067861499485e-005 45 -1.4913067861499485e-005 50 -1.4913067861499485e-005 
		55 -1.4913067861499485e-005 61 -1.4913067861499485e-005 63 -1.4913067861499485e-005;
createNode animCurveTL -n "D_:R_RL_Toe_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 4.7933695242891039e-007 2 4.7933695242891039e-007 
		5 4.7933695242891039e-007 7 4.7933695242891039e-007 9 4.7933695242891039e-007 12 
		4.7933695242891039e-007 18 4.7933695242891039e-007 26 4.7933695242891039e-007 30 
		4.7933695242891039e-007 34 4.7933695242891039e-007 39 4.7933695242891039e-007 41 
		4.7933695242891039e-007 45 4.7933695242891039e-007 50 4.7933695242891039e-007 55 
		4.7933695242891039e-007 61 4.7933695242891039e-007 63 4.7933695242891039e-007;
createNode animCurveTL -n "D_:R_RL_Toe_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -24.692969503474171 2 -24.692969503474171 
		5 -24.692969503474171 7 -24.692969503474171 9 -24.692969503474171 12 -24.692969503474171 
		18 -24.692969503474171 26 -24.692969503474171 30 -24.692969503474171 34 -24.692969503474171 
		39 -24.692969503474171 41 -24.692969503474171 45 -24.692969503474171 50 -24.692969503474171 
		55 -24.692969503474171 61 -24.692969503474171 63 -24.692969503474171;
createNode animCurveTL -n "D_:R_RL_Ball_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 3.5574148071759737e-007 2 3.5574148071759737e-007 
		5 3.5574148071759737e-007 7 3.5574148071759737e-007 9 3.5574148071759737e-007 12 
		3.5574148071759737e-007 18 3.5574148071759737e-007 26 3.5574148071759737e-007 30 
		3.5574148071759737e-007 34 3.5574148071759737e-007 39 3.5574148071759737e-007 41 
		3.5574148071759737e-007 45 3.5574148071759737e-007 50 3.5574148071759737e-007 55 
		3.5574148071759737e-007 61 3.5574148071759737e-007 63 3.5574148071759737e-007;
createNode animCurveTL -n "D_:R_RL_Ball_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1.7292577481758942e-007 2 -1.7292577481758942e-007 
		5 -1.7292577481758942e-007 7 -1.7292577481758942e-007 9 -1.7292577481758942e-007 
		12 -1.7292577481758942e-007 18 -1.7292577481758942e-007 26 -1.7292577481758942e-007 
		30 -1.7292577481758942e-007 34 -1.7292577481758942e-007 39 -1.7292577481758942e-007 
		41 -1.7292577481758942e-007 45 -1.7292577481758942e-007 50 -1.7292577481758942e-007 
		55 -1.7292577481758942e-007 61 -1.7292577481758942e-007 63 -1.7292577481758942e-007;
createNode animCurveTL -n "D_:R_RL_Ball_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 11.979817960079172 2 11.979817960079172 
		5 11.979817960079172 7 11.979817960079172 9 11.979817960079172 12 11.979817960079172 
		18 11.979817960079172 26 11.979817960079172 30 11.979817960079172 34 11.979817960079172 
		39 11.979817960079172 41 11.979817960079172 45 11.979817960079172 50 11.979817960079172 
		55 11.979817960079172 61 11.979817960079172 63 11.979817960079172;
createNode animCurveTL -n "D_:R_RL_Ankle_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -6.9401218869558079e-006 2 -6.9401218869558079e-006 
		5 -6.9401218869558079e-006 7 -6.9401218869558079e-006 9 -6.9401218869558079e-006 
		12 -6.9401218869558079e-006 18 -6.9401218869558079e-006 26 -6.9401218869558079e-006 
		30 -6.9401218869558079e-006 34 -6.9401218869558079e-006 39 -6.9401218869558079e-006 
		41 -6.9401218869558079e-006 45 -6.9401218869558079e-006 50 -6.9401218869558079e-006 
		55 -6.9401218869558079e-006 61 -6.9401218869558079e-006 63 -6.9401218869558079e-006;
createNode animCurveTL -n "D_:R_RL_Ankle_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -13.61343371036873 2 -13.61343371036873 
		5 -13.61343371036873 7 -13.61343371036873 9 -13.61343371036873 12 -13.61343371036873 
		18 -13.61343371036873 26 -13.61343371036873 30 -13.61343371036873 34 -13.61343371036873 
		39 -13.61343371036873 41 -13.61343371036873 45 -13.61343371036873 50 -13.61343371036873 
		55 -13.61343371036873 61 -13.61343371036873 63 -13.61343371036873;
createNode animCurveTL -n "D_:R_RL_Ankle_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 6.1714281095049595 2 6.1714281095049595 
		5 6.1714281095049595 7 6.1714281095049595 9 6.1714281095049595 12 6.1714281095049595 
		18 6.1714281095049595 26 6.1714281095049595 30 6.1714281095049595 34 6.1714281095049595 
		39 6.1714281095049595 41 6.1714281095049595 45 6.1714281095049595 50 6.1714281095049595 
		55 6.1714281095049595 61 6.1714281095049595 63 6.1714281095049595;
createNode animCurveTL -n "D_:ikHandle_r_ankle_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 2.1497448265961339e-005 2 2.1497448265961339e-005 
		5 2.1497448265961339e-005 7 2.1497448265961339e-005 9 2.1497448265961339e-005 12 
		2.1497448265961339e-005 18 2.1497448265961339e-005 26 2.1497448265961339e-005 30 
		2.1497448265961339e-005 34 2.1497448265961339e-005 39 2.1497448265961339e-005 41 
		2.1497448265961339e-005 45 2.1497448265961339e-005 50 2.1497448265961339e-005 55 
		2.1497448265961339e-005 61 2.1497448265961339e-005 63 2.1497448265961339e-005;
createNode animCurveTL -n "D_:ikHandle_r_ankle_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 8.7738518672608734e-007 2 8.7738518672608734e-007 
		5 8.7738518672608734e-007 7 8.7738518672608734e-007 9 8.7738518672608734e-007 12 
		8.7738518672608734e-007 18 8.7738518672608734e-007 26 8.7738518672608734e-007 30 
		8.7738518672608734e-007 34 8.7738518672608734e-007 39 8.7738518672608734e-007 41 
		8.7738518672608734e-007 45 8.7738518672608734e-007 50 8.7738518672608734e-007 55 
		8.7738518672608734e-007 61 8.7738518672608734e-007 63 8.7738518672608734e-007;
createNode animCurveTL -n "D_:ikHandle_r_ankle_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1.7498882329824994e-005 2 -1.7498882329824994e-005 
		5 -1.7498882329824994e-005 7 -1.7498882329824994e-005 9 -1.7498882329824994e-005 
		12 -1.7498882329824994e-005 18 -1.7498882329824994e-005 26 -1.7498882329824994e-005 
		30 -1.7498882329824994e-005 34 -1.7498882329824994e-005 39 -1.7498882329824994e-005 
		41 -1.7498882329824994e-005 45 -1.7498882329824994e-005 50 -1.7498882329824994e-005 
		55 -1.7498882329824994e-005 61 -1.7498882329824994e-005 63 -1.7498882329824994e-005;
createNode animCurveTL -n "D_:ikHandle_r_ball_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1.4852429742973072e-005 2 1.4852429742973072e-005 
		5 1.4852429742973072e-005 7 1.4852429742973072e-005 9 1.4852429742973072e-005 12 
		1.4852429742973072e-005 18 1.4852429742973072e-005 26 1.4852429742973072e-005 30 
		1.4852429742973072e-005 34 1.4852429742973072e-005 39 1.4852429742973072e-005 41 
		1.4852429742973072e-005 45 1.4852429742973072e-005 50 1.4852429742973072e-005 55 
		1.4852429742973072e-005 61 1.4852429742973072e-005 63 1.4852429742973072e-005;
createNode animCurveTL -n "D_:ikHandle_r_ball_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -3.2534714089829664e-007 2 -3.2534714089829664e-007 
		5 -3.2534714089829664e-007 7 -3.2534714089829664e-007 9 -3.2534714089829664e-007 
		12 -3.2534714089829664e-007 18 -3.2534714089829664e-007 26 -3.2534714089829664e-007 
		30 -3.2534714089829664e-007 34 -3.2534714089829664e-007 39 -3.2534714089829664e-007 
		41 -3.2534714089829664e-007 45 -3.2534714089829664e-007 50 -3.2534714089829664e-007 
		55 -3.2534714089829664e-007 61 -3.2534714089829664e-007 63 -3.2534714089829664e-007;
createNode animCurveTL -n "D_:ikHandle_r_ball_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -2.2244810438110107e-008 2 -2.2244810438110107e-008 
		5 -2.2244810438110107e-008 7 -2.2244810438110107e-008 9 -2.2244810438110107e-008 
		12 -2.2244810438110107e-008 18 -2.2244810438110107e-008 26 -2.2244810438110107e-008 
		30 -2.2244810438110107e-008 34 -2.2244810438110107e-008 39 -2.2244810438110107e-008 
		41 -2.2244810438110107e-008 45 -2.2244810438110107e-008 50 -2.2244810438110107e-008 
		55 -2.2244810438110107e-008 61 -2.2244810438110107e-008 63 -2.2244810438110107e-008;
createNode animCurveTL -n "D_:ikHandle_r_toe_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1.584116392727708e-005 2 1.584116392727708e-005 
		5 1.584116392727708e-005 7 1.584116392727708e-005 9 1.584116392727708e-005 12 1.584116392727708e-005 
		18 1.584116392727708e-005 26 1.584116392727708e-005 30 1.584116392727708e-005 34 
		1.584116392727708e-005 39 1.584116392727708e-005 41 1.584116392727708e-005 45 1.584116392727708e-005 
		50 1.584116392727708e-005 55 1.584116392727708e-005 61 1.584116392727708e-005 63 
		1.584116392727708e-005;
createNode pairBlend -n "pairBlend2";
	setAttr ".txm" 2;
	setAttr ".tzm" 2;
	setAttr ".rm" 2;
createNode animCurveTL -n "pairBlend2_inTranslateY1";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:ikHandle_r_toe_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1.2763833970197425e-005 2 1.2763833970197425e-005 
		5 1.2763833970197425e-005 7 1.2763833970197425e-005 9 1.2763833970197425e-005 12 
		1.2763833970197425e-005 18 1.2763833970197425e-005 26 1.2763833970197425e-005 30 
		1.2763833970197425e-005 34 1.2763833970197425e-005 39 1.2763833970197425e-005 41 
		1.2763833970197425e-005 45 1.2763833970197425e-005 50 1.2763833970197425e-005 55 
		1.2763833970197425e-005 61 1.2763833970197425e-005 63 1.2763833970197425e-005;
createNode animCurveTU -n "D_:L_RL_Base_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:L_RL_Base_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:L_RL_Base_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:L_RL_Base_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:L_RL_Base_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Base_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Base_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Toe_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:L_RL_Toe_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:L_RL_Toe_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:L_RL_Toe_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:L_RL_Toe_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Toe_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Toe_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Ball_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode pairBlend -n "pairBlend3";
	setAttr ".txm" 2;
	setAttr ".tym" 2;
	setAttr ".tzm" 2;
createNode animCurveTA -n "pairBlend3_inRotateX1";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 18 ".ktv[0:17]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 9.624575428388777 45 1.2943989300929772 50 8.7980840742943247 51 -2.8535144533419072 
		55 0 61 0 63 0;
createNode animCurveTA -n "D_:L_RL_Ball_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:L_RL_Ball_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:L_RL_Ball_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Ball_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Ball_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Ankle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:L_RL_Ankle_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:L_RL_Ankle_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:L_RL_Ankle_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:L_RL_Ankle_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Ankle_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:L_RL_Ankle_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ankle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:ikHandle_l_ankle_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_ankle_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_ankle_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_ankle_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ankle_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ankle_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ankle_offset";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_ankle_roll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_ankle_twist";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_ankle_ikBlend";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ball_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:ikHandle_l_ball_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_ball_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_ball_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_ball_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ball_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ball_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ball_poleVectorX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_ball_poleVectorY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_ball_poleVectorZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_ball_offset";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_ball_roll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_ball_twist";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_ball_ikBlend";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_toe_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:ikHandle_l_toe_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_toe_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_toe_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_toe_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_toe_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_toe_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_toe_poleVectorX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_toe_poleVectorY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_toe_poleVectorZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_l_toe_offset";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_toe_roll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_l_toe_twist";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_l_toe_ikBlend";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "LFoot";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:R_RL_Base_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:R_RL_Base_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:R_RL_Base_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:R_RL_Base_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:R_RL_Base_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Base_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Base_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Toe_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:R_RL_Toe_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:R_RL_Toe_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:R_RL_Toe_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:R_RL_Toe_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Toe_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Toe_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Ball_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode pairBlend -n "pairBlend4";
	setAttr ".txm" 2;
	setAttr ".tym" 2;
	setAttr ".tzm" 2;
createNode animCurveTA -n "pairBlend4_inRotateX1";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 11.759642057123928 12 
		19.33965870224835 18 17.113282236248697 26 3.8417642567641779 30 0 34 0 39 0 41 0 
		45 0 50 0 55 0 61 8.4316268076142133 63 0;
createNode animCurveTA -n "D_:R_RL_Ball_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 1.0675629674930152 
		18 0 26 0 30 0 34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:R_RL_Ball_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 -1.3124417024018127 
		18 0 26 0 30 0 34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:R_RL_Ball_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Ball_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Ball_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Ankle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:R_RL_Ankle_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:R_RL_Ankle_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:R_RL_Ankle_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:R_RL_Ankle_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Ankle_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:R_RL_Ankle_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_ankle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:ikHandle_r_ankle_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 180 2 180 5 180 7 180 9 180 12 180 18 
		180 26 180 30 180 34 180 39 180 41 180 45 180 50 180 55 180 61 180 63 180;
createNode animCurveTA -n "D_:ikHandle_r_ankle_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_r_ankle_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_r_ankle_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_ankle_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_ankle_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_ankle_offset";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_r_ankle_roll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_r_ankle_twist";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_r_ankle_ikBlend";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_ball_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:ikHandle_r_ball_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 2.2947740193415445e-007 2 2.2947740193415445e-007 
		5 2.2947740193415445e-007 7 2.2947740193415445e-007 9 2.2947740193415445e-007 12 
		2.2947740193415445e-007 18 2.2947740193415445e-007 26 2.2947740193415445e-007 30 
		2.2947740193415445e-007 34 2.2947740193415445e-007 39 2.2947740193415445e-007 41 
		2.2947740193415445e-007 45 2.2947740193415445e-007 50 2.2947740193415445e-007 55 
		2.2947740193415445e-007 61 2.2947740193415445e-007 63 2.2947740193415445e-007;
createNode animCurveTA -n "D_:ikHandle_r_ball_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -2.7450084752015336e-006 2 -2.7450084752015336e-006 
		5 -2.7450084752015336e-006 7 -2.7450084752015336e-006 9 -2.7450084752015336e-006 
		12 -2.7450084752015336e-006 18 -2.7450084752015336e-006 26 -2.7450084752015336e-006 
		30 -2.7450084752015336e-006 34 -2.7450084752015336e-006 39 -2.7450084752015336e-006 
		41 -2.7450084752015336e-006 45 -2.7450084752015336e-006 50 -2.7450084752015336e-006 
		55 -2.7450084752015336e-006 61 -2.7450084752015336e-006 63 -2.7450084752015336e-006;
createNode animCurveTA -n "D_:ikHandle_r_ball_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 3.3616658427879176e-022 2 3.3616658427879176e-022 
		5 3.3616658427879176e-022 7 3.3616658427879176e-022 9 3.3616658427879176e-022 12 
		3.3616658427879176e-022 18 3.3616658427879176e-022 26 3.3616658427879176e-022 30 
		3.3616658427879176e-022 34 3.3616658427879176e-022 39 3.3616658427879176e-022 41 
		3.3616658427879176e-022 45 3.3616658427879176e-022 50 3.3616658427879176e-022 55 
		3.3616658427879176e-022 61 3.3616658427879176e-022 63 3.3616658427879176e-022;
createNode animCurveTU -n "D_:ikHandle_r_ball_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_ball_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_ball_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1.0000000000000002 2 1.0000000000000002 
		5 1.0000000000000002 7 1.0000000000000002 9 1.0000000000000002 12 1.0000000000000002 
		18 1.0000000000000002 26 1.0000000000000002 30 1.0000000000000002 34 1.0000000000000002 
		39 1.0000000000000002 41 1.0000000000000002 45 1.0000000000000002 50 1.0000000000000002 
		55 1.0000000000000002 61 1.0000000000000002 63 1.0000000000000002;
createNode animCurveTU -n "D_:ikHandle_r_ball_poleVectorX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_r_ball_poleVectorY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1.2246467991473532e-016 2 -1.2246467991473532e-016 
		5 -1.2246467991473532e-016 7 -1.2246467991473532e-016 9 -1.2246467991473532e-016 
		12 -1.2246467991473532e-016 18 -1.2246467991473532e-016 26 -1.2246467991473532e-016 
		30 -1.2246467991473532e-016 34 -1.2246467991473532e-016 39 -1.2246467991473532e-016 
		41 -1.2246467991473532e-016 45 -1.2246467991473532e-016 50 -1.2246467991473532e-016 
		55 -1.2246467991473532e-016 61 -1.2246467991473532e-016 63 -1.2246467991473532e-016;
createNode animCurveTU -n "D_:ikHandle_r_ball_poleVectorZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1 2 -1 5 -1 7 -1 9 -1 12 -1 18 -1 26 
		-1 30 -1 34 -1 39 -1 41 -1 45 -1 50 -1 55 -1 61 -1 63 -1;
createNode animCurveTU -n "D_:ikHandle_r_ball_offset";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_r_ball_roll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_r_ball_twist";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_r_ball_ikBlend";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_toe_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTA -n "D_:ikHandle_r_toe_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 2.2947569715684519e-007 2 2.2947569715684519e-007 
		5 2.2947569715684519e-007 7 2.2947569715684519e-007 9 2.2947569715684519e-007 12 
		2.2947569715684519e-007 18 2.2947569715684519e-007 26 2.2947569715684519e-007 30 
		2.2947569715684519e-007 34 2.2947569715684519e-007 39 2.2947569715684519e-007 41 
		2.2947569715684519e-007 45 2.2947569715684519e-007 50 2.2947569715684519e-007 55 
		2.2947569715684519e-007 61 2.2947569715684519e-007 63 2.2947569715684519e-007;
createNode animCurveTA -n "D_:ikHandle_r_toe_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -2.9315163558400072e-006 2 -2.9315163558400072e-006 
		5 -2.9315163558400072e-006 7 -2.9315163558400072e-006 9 -2.9315163558400072e-006 
		12 -2.9315163558400072e-006 18 -2.9315163558400072e-006 26 -2.9315163558400072e-006 
		30 -2.9315163558400072e-006 34 -2.9315163558400072e-006 39 -2.9315163558400072e-006 
		41 -2.9315163558400072e-006 45 -2.9315163558400072e-006 50 -2.9315163558400072e-006 
		55 -2.9315163558400072e-006 61 -2.9315163558400072e-006 63 -2.9315163558400072e-006;
createNode animCurveTA -n "D_:ikHandle_r_toe_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 3.5900721218275809e-022 2 3.5900721218275809e-022 
		5 3.5900721218275809e-022 7 3.5900721218275809e-022 9 3.5900721218275809e-022 12 
		3.5900721218275809e-022 18 3.5900721218275809e-022 26 3.5900721218275809e-022 30 
		3.5900721218275809e-022 34 3.5900721218275809e-022 39 3.5900721218275809e-022 41 
		3.5900721218275809e-022 45 3.5900721218275809e-022 50 3.5900721218275809e-022 55 
		3.5900721218275809e-022 61 3.5900721218275809e-022 63 3.5900721218275809e-022;
createNode animCurveTU -n "D_:ikHandle_r_toe_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_toe_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_toe_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:ikHandle_r_toe_poleVectorX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_r_toe_poleVectorY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1.2246467991473532e-016 2 -1.2246467991473532e-016 
		5 -1.2246467991473532e-016 7 -1.2246467991473532e-016 9 -1.2246467991473532e-016 
		12 -1.2246467991473532e-016 18 -1.2246467991473532e-016 26 -1.2246467991473532e-016 
		30 -1.2246467991473532e-016 34 -1.2246467991473532e-016 39 -1.2246467991473532e-016 
		41 -1.2246467991473532e-016 45 -1.2246467991473532e-016 50 -1.2246467991473532e-016 
		55 -1.2246467991473532e-016 61 -1.2246467991473532e-016 63 -1.2246467991473532e-016;
createNode animCurveTU -n "D_:ikHandle_r_toe_poleVectorZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -1 2 -1 5 -1 7 -1 9 -1 12 -1 18 -1 26 
		-1 30 -1 34 -1 39 -1 41 -1 45 -1 50 -1 55 -1 61 -1 63 -1;
createNode animCurveTU -n "D_:ikHandle_r_toe_offset";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_r_toe_roll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTA -n "D_:ikHandle_r_toe_twist";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:ikHandle_r_toe_ikBlend";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "RFoot";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:Spine0Control_pointConstraint_nodeState";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:Spine0Control_pointConstraint_target_0__targetWeight";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTL -n "D_:Spine0Control_pointConstraint_offsetX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:Spine0Control_pointConstraint_offsetY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:Spine0Control_pointConstraint_offsetZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:Spine0Control_pointConstraint_spine0W0";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:Spine0Control1_pointConstraint_nodeState";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:Spine0Control1_pointConstraint_target_0__targetWeight";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTL -n "D_:Spine0Control1_pointConstraint_offsetX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:Spine0Control1_pointConstraint_offsetY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -3.1313520993904902 2 -3.1313520993904902 
		5 -3.1313520993904902 7 -3.1313520993904902 9 -3.1313520993904902 12 -3.1313520993904902 
		18 -3.1313520993904902 26 -3.1313520993904902 30 -3.1313520993904902 34 -3.1313520993904902 
		39 -3.1313520993904902 41 -3.1313520993904902 45 -3.1313520993904902 50 -3.1313520993904902 
		55 -3.1313520993904902 61 -3.1313520993904902 63 -3.1313520993904902;
createNode animCurveTL -n "D_:Spine0Control1_pointConstraint_offsetZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0.31033644578437181 2 0.31033644578437181 
		5 0.31033644578437181 7 0.31033644578437181 9 0.31033644578437181 12 0.31033644578437181 
		18 0.31033644578437181 26 0.31033644578437181 30 0.31033644578437181 34 0.31033644578437181 
		39 0.31033644578437181 41 0.31033644578437181 45 0.31033644578437181 50 0.31033644578437181 
		55 0.31033644578437181 61 0.31033644578437181 63 0.31033644578437181;
createNode animCurveTU -n "D_:Spine0Control1_pointConstraint_spine1W0";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:NeckControl_pointConstraint_nodeState";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:NeckControl_pointConstraint_target_0__targetWeight";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTL -n "D_:NeckControl_pointConstraint_offsetX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:NeckControl_pointConstraint_offsetY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTL -n "D_:NeckControl_pointConstraint_offsetZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:NeckControl_pointConstraint_neckW0";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "D_:HipControl_pointConstraint_nodeState";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
	setAttr -s 17 ".kot[0:16]"  5 5 5 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:HipControl_pointConstraint_target_0__targetWeight";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTL -n "D_:HipControl_pointConstraint_offsetX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 -4.3368086899420177e-019 2 -4.3368086899420177e-019 
		5 -4.3368086899420177e-019 7 -4.3368086899420177e-019 9 -4.3368086899420177e-019 
		12 -4.3368086899420177e-019 18 -4.3368086899420177e-019 26 -4.3368086899420177e-019 
		30 -4.3368086899420177e-019 34 -4.3368086899420177e-019 39 -4.3368086899420177e-019 
		41 -4.3368086899420177e-019 45 -4.3368086899420177e-019 50 -4.3368086899420177e-019 
		55 -4.3368086899420177e-019 61 -4.3368086899420177e-019 63 -4.3368086899420177e-019;
createNode animCurveTL -n "D_:HipControl_pointConstraint_offsetY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1.7763568394002505e-015 2 1.7763568394002505e-015 
		5 1.7763568394002505e-015 7 1.7763568394002505e-015 9 1.7763568394002505e-015 12 
		1.7763568394002505e-015 18 1.7763568394002505e-015 26 1.7763568394002505e-015 30 
		1.7763568394002505e-015 34 1.7763568394002505e-015 39 1.7763568394002505e-015 41 
		1.7763568394002505e-015 45 1.7763568394002505e-015 50 1.7763568394002505e-015 55 
		1.7763568394002505e-015 61 1.7763568394002505e-015 63 1.7763568394002505e-015;
createNode animCurveTL -n "D_:HipControl_pointConstraint_offsetZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 0 2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 
		34 0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:HipControl_pointConstraint_hipW0";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 2 1 5 1 7 1 9 1 12 1 18 1 26 1 30 1 
		34 1 39 1 41 1 45 1 50 1 55 1 61 1 63 1;
createNode animCurveTU -n "unitConversion1";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 34 
		0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "unitConversion2";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  2 0 5 0 7 0 9 0 12 0 18 0 26 0 30 0 34 
		0 39 0 41 0 45 0 50 0 55 0 61 0 63 0;
createNode animCurveTU -n "D_:r_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".kot[0]"  5;
createNode animCurveTA -n "D_:r_point_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 0 9 3.9525613623514158;
createNode animCurveTA -n "D_:r_point_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 0 9 -5.5954581933344079;
createNode animCurveTA -n "D_:r_point_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 -26.524519 9 -32.216449518755162;
select -ne :time1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".o" 16;
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
connectAttr "D_:Entity_translateX.o" "D_RN.phl[3466]";
connectAttr "D_:Entity_translateY.o" "D_RN.phl[3467]";
connectAttr "D_:Entity_translateZ.o" "D_RN.phl[3468]";
connectAttr "D_:Entity_rotateX.o" "D_RN.phl[3469]";
connectAttr "D_:Entity_rotateY.o" "D_RN.phl[3470]";
connectAttr "D_:Entity_rotateZ.o" "D_RN.phl[3471]";
connectAttr "D_:Entity_visibility.o" "D_RN.phl[3472]";
connectAttr "D_:Entity_scaleX.o" "D_RN.phl[3473]";
connectAttr "D_:Entity_scaleY.o" "D_RN.phl[3474]";
connectAttr "D_:Entity_scaleZ.o" "D_RN.phl[3475]";
connectAttr "D_:DiverGlobal_translateX.o" "D_RN.phl[3476]";
connectAttr "D_:DiverGlobal_translateY.o" "D_RN.phl[3477]";
connectAttr "D_:DiverGlobal_translateZ.o" "D_RN.phl[3478]";
connectAttr "D_:DiverGlobal_rotateX.o" "D_RN.phl[3479]";
connectAttr "D_:DiverGlobal_rotateY.o" "D_RN.phl[3480]";
connectAttr "D_:DiverGlobal_rotateZ.o" "D_RN.phl[3481]";
connectAttr "D_:DiverGlobal_scaleX.o" "D_RN.phl[3482]";
connectAttr "D_:DiverGlobal_scaleY.o" "D_RN.phl[3483]";
connectAttr "D_:DiverGlobal_scaleZ.o" "D_RN.phl[3484]";
connectAttr "D_:DiverGlobal_visibility.o" "D_RN.phl[3485]";
connectAttr "D_:l_mid_1_rotateX.o" "D_RN.phl[3486]";
connectAttr "D_:l_mid_1_rotateY.o" "D_RN.phl[3487]";
connectAttr "D_:l_mid_1_rotateZ.o" "D_RN.phl[3488]";
connectAttr "D_:l_mid_1_visibility.o" "D_RN.phl[3489]";
connectAttr "D_:l_mid_2_rotateX.o" "D_RN.phl[3490]";
connectAttr "D_:l_mid_2_rotateY.o" "D_RN.phl[3491]";
connectAttr "D_:l_mid_2_rotateZ.o" "D_RN.phl[3492]";
connectAttr "D_:l_mid_2_visibility.o" "D_RN.phl[3493]";
connectAttr "D_:l_pink_1_rotateX.o" "D_RN.phl[3494]";
connectAttr "D_:l_pink_1_rotateY.o" "D_RN.phl[3495]";
connectAttr "D_:l_pink_1_rotateZ.o" "D_RN.phl[3496]";
connectAttr "D_:l_pink_1_visibility.o" "D_RN.phl[3497]";
connectAttr "D_:l_pink_2_rotateX.o" "D_RN.phl[3498]";
connectAttr "D_:l_pink_2_rotateY.o" "D_RN.phl[3499]";
connectAttr "D_:l_pink_2_rotateZ.o" "D_RN.phl[3500]";
connectAttr "D_:l_pink_2_visibility.o" "D_RN.phl[3501]";
connectAttr "D_:l_point_1_rotateX.o" "D_RN.phl[3502]";
connectAttr "D_:l_point_1_rotateY.o" "D_RN.phl[3503]";
connectAttr "D_:l_point_1_rotateZ.o" "D_RN.phl[3504]";
connectAttr "D_:l_point_1_visibility.o" "D_RN.phl[3505]";
connectAttr "D_:l_point_2_rotateX.o" "D_RN.phl[3506]";
connectAttr "D_:l_point_2_rotateY.o" "D_RN.phl[3507]";
connectAttr "D_:l_point_2_rotateZ.o" "D_RN.phl[3508]";
connectAttr "D_:l_point_2_visibility.o" "D_RN.phl[3509]";
connectAttr "D_:l_thumb_1_rotateX.o" "D_RN.phl[3510]";
connectAttr "D_:l_thumb_1_rotateY.o" "D_RN.phl[3511]";
connectAttr "D_:l_thumb_1_rotateZ.o" "D_RN.phl[3512]";
connectAttr "D_:l_thumb_1_visibility.o" "D_RN.phl[3513]";
connectAttr "D_:l_thumb_2_rotateX.o" "D_RN.phl[3514]";
connectAttr "D_:l_thumb_2_rotateY.o" "D_RN.phl[3515]";
connectAttr "D_:l_thumb_2_rotateZ.o" "D_RN.phl[3516]";
connectAttr "D_:l_thumb_2_visibility.o" "D_RN.phl[3517]";
connectAttr "D_:r_mid_1_rotateX.o" "D_RN.phl[3518]";
connectAttr "D_:r_mid_1_rotateY.o" "D_RN.phl[3519]";
connectAttr "D_:r_mid_1_rotateZ.o" "D_RN.phl[3520]";
connectAttr "D_:r_mid_1_visibility.o" "D_RN.phl[3521]";
connectAttr "D_:r_mid_2_rotateX.o" "D_RN.phl[3522]";
connectAttr "D_:r_mid_2_rotateY.o" "D_RN.phl[3523]";
connectAttr "D_:r_mid_2_rotateZ.o" "D_RN.phl[3524]";
connectAttr "D_:r_mid_2_visibility.o" "D_RN.phl[3525]";
connectAttr "D_:r_pink_1_rotateX.o" "D_RN.phl[3526]";
connectAttr "D_:r_pink_1_rotateY.o" "D_RN.phl[3527]";
connectAttr "D_:r_pink_1_rotateZ.o" "D_RN.phl[3528]";
connectAttr "D_:r_pink_1_visibility.o" "D_RN.phl[3529]";
connectAttr "D_:r_pink_2_rotateX.o" "D_RN.phl[3530]";
connectAttr "D_:r_pink_2_rotateY.o" "D_RN.phl[3531]";
connectAttr "D_:r_pink_2_rotateZ.o" "D_RN.phl[3532]";
connectAttr "D_:r_pink_2_visibility.o" "D_RN.phl[3533]";
connectAttr "D_:r_point_1_rotateX.o" "D_RN.phl[3534]";
connectAttr "D_:r_point_1_rotateY.o" "D_RN.phl[3535]";
connectAttr "D_:r_point_1_rotateZ.o" "D_RN.phl[3536]";
connectAttr "D_:r_point_1_visibility.o" "D_RN.phl[3537]";
connectAttr "D_:r_point_2_rotateX.o" "D_RN.phl[3538]";
connectAttr "D_:r_point_2_rotateY.o" "D_RN.phl[3539]";
connectAttr "D_:r_point_2_rotateZ.o" "D_RN.phl[3540]";
connectAttr "D_:r_point_2_visibility.o" "D_RN.phl[3541]";
connectAttr "D_:r_thumb_1_rotateX.o" "D_RN.phl[3542]";
connectAttr "D_:r_thumb_1_rotateY.o" "D_RN.phl[3543]";
connectAttr "D_:r_thumb_1_rotateZ.o" "D_RN.phl[3544]";
connectAttr "D_:r_thumb_1_visibility.o" "D_RN.phl[3545]";
connectAttr "D_:r_thumb_2_rotateX.o" "D_RN.phl[3546]";
connectAttr "D_:r_thumb_2_rotateY.o" "D_RN.phl[3547]";
connectAttr "D_:r_thumb_2_rotateZ.o" "D_RN.phl[3548]";
connectAttr "D_:r_thumb_2_visibility.o" "D_RN.phl[3549]";
connectAttr "D_:L_Foot_ToeRoll.o" "D_RN.phl[3550]";
connectAttr "D_RN.phl[3551]" "pairBlend1.ity2";
connectAttr "D_:L_Foot_BallRoll.o" "D_RN.phl[3552]";
connectAttr "D_:L_Foot_translateX.o" "D_RN.phl[3553]";
connectAttr "D_:L_Foot_translateY.o" "D_RN.phl[3554]";
connectAttr "D_:L_Foot_translateZ.o" "D_RN.phl[3555]";
connectAttr "D_:L_Foot_rotateX.o" "D_RN.phl[3556]";
connectAttr "D_:L_Foot_rotateY.o" "D_RN.phl[3557]";
connectAttr "D_:L_Foot_rotateZ.o" "D_RN.phl[3558]";
connectAttr "D_:L_Foot_scaleX.o" "D_RN.phl[3559]";
connectAttr "D_:L_Foot_scaleY.o" "D_RN.phl[3560]";
connectAttr "D_:L_Foot_scaleZ.o" "D_RN.phl[3561]";
connectAttr "D_:L_Foot_visibility.o" "D_RN.phl[3562]";
connectAttr "D_:L_RL_Base_scaleX.o" "D_RN.phl[3563]";
connectAttr "D_:L_RL_Base_scaleY.o" "D_RN.phl[3564]";
connectAttr "D_:L_RL_Base_scaleZ.o" "D_RN.phl[3565]";
connectAttr "D_:L_RL_Base_translateX.o" "D_RN.phl[3566]";
connectAttr "D_:L_RL_Base_translateY.o" "D_RN.phl[3567]";
connectAttr "D_:L_RL_Base_translateZ.o" "D_RN.phl[3568]";
connectAttr "D_:L_RL_Base_visibility.o" "D_RN.phl[3569]";
connectAttr "D_:L_RL_Base_rotateX.o" "D_RN.phl[3570]";
connectAttr "D_:L_RL_Base_rotateY.o" "D_RN.phl[3571]";
connectAttr "D_:L_RL_Base_rotateZ.o" "D_RN.phl[3572]";
connectAttr "D_:L_RL_Toe_scaleX.o" "D_RN.phl[3573]";
connectAttr "D_:L_RL_Toe_scaleY.o" "D_RN.phl[3574]";
connectAttr "D_:L_RL_Toe_scaleZ.o" "D_RN.phl[3575]";
connectAttr "D_:L_RL_Toe_translateX.o" "D_RN.phl[3576]";
connectAttr "D_:L_RL_Toe_translateY.o" "D_RN.phl[3577]";
connectAttr "D_:L_RL_Toe_translateZ.o" "D_RN.phl[3578]";
connectAttr "D_:L_RL_Toe_visibility.o" "D_RN.phl[3579]";
connectAttr "D_:L_RL_Toe_rotateX.o" "D_RN.phl[3580]";
connectAttr "D_:L_RL_Toe_rotateY.o" "D_RN.phl[3581]";
connectAttr "D_:L_RL_Toe_rotateZ.o" "D_RN.phl[3582]";
connectAttr "pairBlend3.orx" "D_RN.phl[3583]";
connectAttr "D_:L_RL_Ball_rotateY.o" "D_RN.phl[3584]";
connectAttr "D_:L_RL_Ball_rotateZ.o" "D_RN.phl[3585]";
connectAttr "D_:L_RL_Ball_scaleX.o" "D_RN.phl[3586]";
connectAttr "D_:L_RL_Ball_scaleY.o" "D_RN.phl[3587]";
connectAttr "D_:L_RL_Ball_scaleZ.o" "D_RN.phl[3588]";
connectAttr "D_:L_RL_Ball_translateX.o" "D_RN.phl[3589]";
connectAttr "D_:L_RL_Ball_translateY.o" "D_RN.phl[3590]";
connectAttr "D_:L_RL_Ball_translateZ.o" "D_RN.phl[3591]";
connectAttr "D_:L_RL_Ball_visibility.o" "D_RN.phl[3592]";
connectAttr "D_:L_RL_Ankle_translateX.o" "D_RN.phl[3593]";
connectAttr "D_:L_RL_Ankle_translateY.o" "D_RN.phl[3594]";
connectAttr "D_:L_RL_Ankle_translateZ.o" "D_RN.phl[3595]";
connectAttr "D_:L_RL_Ankle_visibility.o" "D_RN.phl[3596]";
connectAttr "D_:L_RL_Ankle_rotateX.o" "D_RN.phl[3597]";
connectAttr "D_:L_RL_Ankle_rotateY.o" "D_RN.phl[3598]";
connectAttr "D_:L_RL_Ankle_rotateZ.o" "D_RN.phl[3599]";
connectAttr "D_:L_RL_Ankle_scaleX.o" "D_RN.phl[3600]";
connectAttr "D_:L_RL_Ankle_scaleY.o" "D_RN.phl[3601]";
connectAttr "D_:L_RL_Ankle_scaleZ.o" "D_RN.phl[3602]";
connectAttr "D_:ikHandle_l_ankle_translateX.o" "D_RN.phl[3603]";
connectAttr "D_:ikHandle_l_ankle_translateY.o" "D_RN.phl[3604]";
connectAttr "D_:ikHandle_l_ankle_translateZ.o" "D_RN.phl[3605]";
connectAttr "D_:ikHandle_l_ankle_visibility.o" "D_RN.phl[3606]";
connectAttr "D_:ikHandle_l_ankle_rotateX.o" "D_RN.phl[3607]";
connectAttr "D_:ikHandle_l_ankle_rotateY.o" "D_RN.phl[3608]";
connectAttr "D_:ikHandle_l_ankle_rotateZ.o" "D_RN.phl[3609]";
connectAttr "D_:ikHandle_l_ankle_scaleX.o" "D_RN.phl[3610]";
connectAttr "D_:ikHandle_l_ankle_scaleY.o" "D_RN.phl[3611]";
connectAttr "D_:ikHandle_l_ankle_scaleZ.o" "D_RN.phl[3612]";
connectAttr "D_:ikHandle_l_ankle_offset.o" "D_RN.phl[3613]";
connectAttr "D_:ikHandle_l_ankle_roll.o" "D_RN.phl[3614]";
connectAttr "D_:ikHandle_l_ankle_twist.o" "D_RN.phl[3615]";
connectAttr "D_:ikHandle_l_ankle_ikBlend.o" "D_RN.phl[3616]";
connectAttr "D_:ikHandle_l_ball_translateX.o" "D_RN.phl[3617]";
connectAttr "D_:ikHandle_l_ball_translateY.o" "D_RN.phl[3618]";
connectAttr "D_:ikHandle_l_ball_translateZ.o" "D_RN.phl[3619]";
connectAttr "D_:ikHandle_l_ball_visibility.o" "D_RN.phl[3620]";
connectAttr "D_:ikHandle_l_ball_rotateX.o" "D_RN.phl[3621]";
connectAttr "D_:ikHandle_l_ball_rotateY.o" "D_RN.phl[3622]";
connectAttr "D_:ikHandle_l_ball_rotateZ.o" "D_RN.phl[3623]";
connectAttr "D_:ikHandle_l_ball_scaleX.o" "D_RN.phl[3624]";
connectAttr "D_:ikHandle_l_ball_scaleY.o" "D_RN.phl[3625]";
connectAttr "D_:ikHandle_l_ball_scaleZ.o" "D_RN.phl[3626]";
connectAttr "D_:ikHandle_l_ball_poleVectorX.o" "D_RN.phl[3627]";
connectAttr "D_:ikHandle_l_ball_poleVectorY.o" "D_RN.phl[3628]";
connectAttr "D_:ikHandle_l_ball_poleVectorZ.o" "D_RN.phl[3629]";
connectAttr "D_:ikHandle_l_ball_offset.o" "D_RN.phl[3630]";
connectAttr "D_:ikHandle_l_ball_roll.o" "D_RN.phl[3631]";
connectAttr "D_:ikHandle_l_ball_twist.o" "D_RN.phl[3632]";
connectAttr "D_:ikHandle_l_ball_ikBlend.o" "D_RN.phl[3633]";
connectAttr "pairBlend1.oty" "D_RN.phl[3634]";
connectAttr "D_:ikHandle_l_toe_translateX.o" "D_RN.phl[3635]";
connectAttr "D_:ikHandle_l_toe_translateZ.o" "D_RN.phl[3636]";
connectAttr "D_:ikHandle_l_toe_visibility.o" "D_RN.phl[3637]";
connectAttr "D_:ikHandle_l_toe_rotateX.o" "D_RN.phl[3638]";
connectAttr "D_:ikHandle_l_toe_rotateY.o" "D_RN.phl[3639]";
connectAttr "D_:ikHandle_l_toe_rotateZ.o" "D_RN.phl[3640]";
connectAttr "D_:ikHandle_l_toe_scaleX.o" "D_RN.phl[3641]";
connectAttr "D_:ikHandle_l_toe_scaleY.o" "D_RN.phl[3642]";
connectAttr "D_:ikHandle_l_toe_scaleZ.o" "D_RN.phl[3643]";
connectAttr "D_:ikHandle_l_toe_poleVectorX.o" "D_RN.phl[3644]";
connectAttr "D_:ikHandle_l_toe_poleVectorY.o" "D_RN.phl[3645]";
connectAttr "D_:ikHandle_l_toe_poleVectorZ.o" "D_RN.phl[3646]";
connectAttr "D_:ikHandle_l_toe_offset.o" "D_RN.phl[3647]";
connectAttr "D_:ikHandle_l_toe_roll.o" "D_RN.phl[3648]";
connectAttr "D_:ikHandle_l_toe_twist.o" "D_RN.phl[3649]";
connectAttr "D_:ikHandle_l_toe_ikBlend.o" "D_RN.phl[3650]";
connectAttr "D_:R_Foot_ToeRoll.o" "D_RN.phl[3651]";
connectAttr "D_RN.phl[3652]" "pairBlend2.ity2";
connectAttr "D_:R_Foot_BallRoll.o" "D_RN.phl[3653]";
connectAttr "D_:R_Foot_translateX.o" "D_RN.phl[3654]";
connectAttr "D_:R_Foot_translateY.o" "D_RN.phl[3655]";
connectAttr "D_:R_Foot_translateZ.o" "D_RN.phl[3656]";
connectAttr "D_:R_Foot_rotateX.o" "D_RN.phl[3657]";
connectAttr "D_:R_Foot_rotateY.o" "D_RN.phl[3658]";
connectAttr "D_:R_Foot_rotateZ.o" "D_RN.phl[3659]";
connectAttr "D_:R_Foot_scaleX.o" "D_RN.phl[3660]";
connectAttr "D_:R_Foot_scaleY.o" "D_RN.phl[3661]";
connectAttr "D_:R_Foot_scaleZ.o" "D_RN.phl[3662]";
connectAttr "D_:R_Foot_visibility.o" "D_RN.phl[3663]";
connectAttr "D_:R_RL_Base_scaleX.o" "D_RN.phl[3664]";
connectAttr "D_:R_RL_Base_scaleY.o" "D_RN.phl[3665]";
connectAttr "D_:R_RL_Base_scaleZ.o" "D_RN.phl[3666]";
connectAttr "D_:R_RL_Base_rotateX.o" "D_RN.phl[3667]";
connectAttr "D_:R_RL_Base_rotateY.o" "D_RN.phl[3668]";
connectAttr "D_:R_RL_Base_rotateZ.o" "D_RN.phl[3669]";
connectAttr "D_:R_RL_Base_translateX.o" "D_RN.phl[3670]";
connectAttr "D_:R_RL_Base_translateY.o" "D_RN.phl[3671]";
connectAttr "D_:R_RL_Base_translateZ.o" "D_RN.phl[3672]";
connectAttr "D_:R_RL_Base_visibility.o" "D_RN.phl[3673]";
connectAttr "D_:R_RL_Toe_scaleX.o" "D_RN.phl[3674]";
connectAttr "D_:R_RL_Toe_scaleY.o" "D_RN.phl[3675]";
connectAttr "D_:R_RL_Toe_scaleZ.o" "D_RN.phl[3676]";
connectAttr "D_:R_RL_Toe_translateX.o" "D_RN.phl[3677]";
connectAttr "D_:R_RL_Toe_translateY.o" "D_RN.phl[3678]";
connectAttr "D_:R_RL_Toe_translateZ.o" "D_RN.phl[3679]";
connectAttr "D_:R_RL_Toe_visibility.o" "D_RN.phl[3680]";
connectAttr "D_:R_RL_Toe_rotateX.o" "D_RN.phl[3681]";
connectAttr "D_:R_RL_Toe_rotateY.o" "D_RN.phl[3682]";
connectAttr "D_:R_RL_Toe_rotateZ.o" "D_RN.phl[3683]";
connectAttr "pairBlend4.orx" "D_RN.phl[3684]";
connectAttr "D_:R_RL_Ball_rotateY.o" "D_RN.phl[3685]";
connectAttr "D_:R_RL_Ball_rotateZ.o" "D_RN.phl[3686]";
connectAttr "D_:R_RL_Ball_scaleX.o" "D_RN.phl[3687]";
connectAttr "D_:R_RL_Ball_scaleY.o" "D_RN.phl[3688]";
connectAttr "D_:R_RL_Ball_scaleZ.o" "D_RN.phl[3689]";
connectAttr "D_:R_RL_Ball_translateX.o" "D_RN.phl[3690]";
connectAttr "D_:R_RL_Ball_translateY.o" "D_RN.phl[3691]";
connectAttr "D_:R_RL_Ball_translateZ.o" "D_RN.phl[3692]";
connectAttr "D_:R_RL_Ball_visibility.o" "D_RN.phl[3693]";
connectAttr "D_:R_RL_Ankle_translateX.o" "D_RN.phl[3694]";
connectAttr "D_:R_RL_Ankle_translateY.o" "D_RN.phl[3695]";
connectAttr "D_:R_RL_Ankle_translateZ.o" "D_RN.phl[3696]";
connectAttr "D_:R_RL_Ankle_visibility.o" "D_RN.phl[3697]";
connectAttr "D_:R_RL_Ankle_rotateX.o" "D_RN.phl[3698]";
connectAttr "D_:R_RL_Ankle_rotateY.o" "D_RN.phl[3699]";
connectAttr "D_:R_RL_Ankle_rotateZ.o" "D_RN.phl[3700]";
connectAttr "D_:R_RL_Ankle_scaleX.o" "D_RN.phl[3701]";
connectAttr "D_:R_RL_Ankle_scaleY.o" "D_RN.phl[3702]";
connectAttr "D_:R_RL_Ankle_scaleZ.o" "D_RN.phl[3703]";
connectAttr "D_:ikHandle_r_ankle_translateX.o" "D_RN.phl[3704]";
connectAttr "D_:ikHandle_r_ankle_translateY.o" "D_RN.phl[3705]";
connectAttr "D_:ikHandle_r_ankle_translateZ.o" "D_RN.phl[3706]";
connectAttr "D_:ikHandle_r_ankle_visibility.o" "D_RN.phl[3707]";
connectAttr "D_:ikHandle_r_ankle_rotateX.o" "D_RN.phl[3708]";
connectAttr "D_:ikHandle_r_ankle_rotateY.o" "D_RN.phl[3709]";
connectAttr "D_:ikHandle_r_ankle_rotateZ.o" "D_RN.phl[3710]";
connectAttr "D_:ikHandle_r_ankle_scaleX.o" "D_RN.phl[3711]";
connectAttr "D_:ikHandle_r_ankle_scaleY.o" "D_RN.phl[3712]";
connectAttr "D_:ikHandle_r_ankle_scaleZ.o" "D_RN.phl[3713]";
connectAttr "D_:ikHandle_r_ankle_offset.o" "D_RN.phl[3714]";
connectAttr "D_:ikHandle_r_ankle_roll.o" "D_RN.phl[3715]";
connectAttr "D_:ikHandle_r_ankle_twist.o" "D_RN.phl[3716]";
connectAttr "D_:ikHandle_r_ankle_ikBlend.o" "D_RN.phl[3717]";
connectAttr "D_:ikHandle_r_ball_translateX.o" "D_RN.phl[3718]";
connectAttr "D_:ikHandle_r_ball_translateY.o" "D_RN.phl[3719]";
connectAttr "D_:ikHandle_r_ball_translateZ.o" "D_RN.phl[3720]";
connectAttr "D_:ikHandle_r_ball_visibility.o" "D_RN.phl[3721]";
connectAttr "D_:ikHandle_r_ball_rotateX.o" "D_RN.phl[3722]";
connectAttr "D_:ikHandle_r_ball_rotateY.o" "D_RN.phl[3723]";
connectAttr "D_:ikHandle_r_ball_rotateZ.o" "D_RN.phl[3724]";
connectAttr "D_:ikHandle_r_ball_scaleX.o" "D_RN.phl[3725]";
connectAttr "D_:ikHandle_r_ball_scaleY.o" "D_RN.phl[3726]";
connectAttr "D_:ikHandle_r_ball_scaleZ.o" "D_RN.phl[3727]";
connectAttr "D_:ikHandle_r_ball_poleVectorX.o" "D_RN.phl[3728]";
connectAttr "D_:ikHandle_r_ball_poleVectorY.o" "D_RN.phl[3729]";
connectAttr "D_:ikHandle_r_ball_poleVectorZ.o" "D_RN.phl[3730]";
connectAttr "D_:ikHandle_r_ball_offset.o" "D_RN.phl[3731]";
connectAttr "D_:ikHandle_r_ball_roll.o" "D_RN.phl[3732]";
connectAttr "D_:ikHandle_r_ball_twist.o" "D_RN.phl[3733]";
connectAttr "D_:ikHandle_r_ball_ikBlend.o" "D_RN.phl[3734]";
connectAttr "pairBlend2.oty" "D_RN.phl[3735]";
connectAttr "D_:ikHandle_r_toe_translateX.o" "D_RN.phl[3736]";
connectAttr "D_:ikHandle_r_toe_translateZ.o" "D_RN.phl[3737]";
connectAttr "D_:ikHandle_r_toe_visibility.o" "D_RN.phl[3738]";
connectAttr "D_:ikHandle_r_toe_rotateX.o" "D_RN.phl[3739]";
connectAttr "D_:ikHandle_r_toe_rotateY.o" "D_RN.phl[3740]";
connectAttr "D_:ikHandle_r_toe_rotateZ.o" "D_RN.phl[3741]";
connectAttr "D_:ikHandle_r_toe_scaleX.o" "D_RN.phl[3742]";
connectAttr "D_:ikHandle_r_toe_scaleY.o" "D_RN.phl[3743]";
connectAttr "D_:ikHandle_r_toe_scaleZ.o" "D_RN.phl[3744]";
connectAttr "D_:ikHandle_r_toe_poleVectorX.o" "D_RN.phl[3745]";
connectAttr "D_:ikHandle_r_toe_poleVectorY.o" "D_RN.phl[3746]";
connectAttr "D_:ikHandle_r_toe_poleVectorZ.o" "D_RN.phl[3747]";
connectAttr "D_:ikHandle_r_toe_offset.o" "D_RN.phl[3748]";
connectAttr "D_:ikHandle_r_toe_roll.o" "D_RN.phl[3749]";
connectAttr "D_:ikHandle_r_toe_twist.o" "D_RN.phl[3750]";
connectAttr "D_:ikHandle_r_toe_ikBlend.o" "D_RN.phl[3751]";
connectAttr "D_:L_Knee_translateX.o" "D_RN.phl[3752]";
connectAttr "D_:L_Knee_translateY.o" "D_RN.phl[3753]";
connectAttr "D_:L_Knee_translateZ.o" "D_RN.phl[3754]";
connectAttr "D_:L_Knee_scaleX.o" "D_RN.phl[3755]";
connectAttr "D_:L_Knee_scaleY.o" "D_RN.phl[3756]";
connectAttr "D_:L_Knee_scaleZ.o" "D_RN.phl[3757]";
connectAttr "D_:L_Knee_visibility.o" "D_RN.phl[3758]";
connectAttr "D_:R_Knee_translateX.o" "D_RN.phl[3759]";
connectAttr "D_:R_Knee_translateY.o" "D_RN.phl[3760]";
connectAttr "D_:R_Knee_translateZ.o" "D_RN.phl[3761]";
connectAttr "D_:R_Knee_scaleX.o" "D_RN.phl[3762]";
connectAttr "D_:R_Knee_scaleY.o" "D_RN.phl[3763]";
connectAttr "D_:R_Knee_scaleZ.o" "D_RN.phl[3764]";
connectAttr "D_:R_Knee_visibility.o" "D_RN.phl[3765]";
connectAttr "D_:RootControl_translateX.o" "D_RN.phl[3766]";
connectAttr "D_:RootControl_translateY.o" "D_RN.phl[3767]";
connectAttr "D_:RootControl_translateZ.o" "D_RN.phl[3768]";
connectAttr "D_:RootControl_rotateX.o" "D_RN.phl[3769]";
connectAttr "D_:RootControl_rotateY.o" "D_RN.phl[3770]";
connectAttr "D_:RootControl_rotateZ.o" "D_RN.phl[3771]";
connectAttr "D_:RootControl_scaleX.o" "D_RN.phl[3772]";
connectAttr "D_:RootControl_scaleY.o" "D_RN.phl[3773]";
connectAttr "D_:RootControl_scaleZ.o" "D_RN.phl[3774]";
connectAttr "D_:RootControl_visibility.o" "D_RN.phl[3775]";
connectAttr "D_:Spine0Control_translateX.o" "D_RN.phl[3776]";
connectAttr "D_:Spine0Control_translateY.o" "D_RN.phl[3777]";
connectAttr "D_:Spine0Control_translateZ.o" "D_RN.phl[3778]";
connectAttr "D_:Spine0Control_scaleX.o" "D_RN.phl[3779]";
connectAttr "D_:Spine0Control_scaleY.o" "D_RN.phl[3780]";
connectAttr "D_:Spine0Control_scaleZ.o" "D_RN.phl[3781]";
connectAttr "D_:Spine0Control_rotateX.o" "D_RN.phl[3782]";
connectAttr "D_:Spine0Control_rotateY.o" "D_RN.phl[3783]";
connectAttr "D_:Spine0Control_rotateZ.o" "D_RN.phl[3784]";
connectAttr "D_:Spine0Control_visibility.o" "D_RN.phl[3785]";
connectAttr "D_:Spine0Control_pointConstraint_nodeState.o" "D_RN.phl[3786]";
connectAttr "D_:Spine0Control_pointConstraint_target_0__targetWeight.o" "D_RN.phl[3787]"
		;
connectAttr "D_:Spine0Control_pointConstraint_offsetX.o" "D_RN.phl[3788]";
connectAttr "D_:Spine0Control_pointConstraint_offsetY.o" "D_RN.phl[3789]";
connectAttr "D_:Spine0Control_pointConstraint_offsetZ.o" "D_RN.phl[3790]";
connectAttr "D_:Spine0Control_pointConstraint_spine0W0.o" "D_RN.phl[3791]";
connectAttr "D_:Spine1Control_scaleX.o" "D_RN.phl[3792]";
connectAttr "D_:Spine1Control_scaleY.o" "D_RN.phl[3793]";
connectAttr "D_:Spine1Control_scaleZ.o" "D_RN.phl[3794]";
connectAttr "D_:Spine1Control_rotateX.o" "D_RN.phl[3795]";
connectAttr "D_:Spine1Control_rotateY.o" "D_RN.phl[3796]";
connectAttr "D_:Spine1Control_rotateZ.o" "D_RN.phl[3797]";
connectAttr "D_:Spine1Control_visibility.o" "D_RN.phl[3798]";
connectAttr "D_:Spine0Control1_pointConstraint_nodeState.o" "D_RN.phl[3799]";
connectAttr "D_:Spine0Control1_pointConstraint_target_0__targetWeight.o" "D_RN.phl[3800]"
		;
connectAttr "D_:Spine0Control1_pointConstraint_offsetX.o" "D_RN.phl[3801]";
connectAttr "D_:Spine0Control1_pointConstraint_offsetY.o" "D_RN.phl[3802]";
connectAttr "D_:Spine0Control1_pointConstraint_offsetZ.o" "D_RN.phl[3803]";
connectAttr "D_:Spine0Control1_pointConstraint_spine1W0.o" "D_RN.phl[3804]";
connectAttr "D_:TankControl_rotateX.o" "D_RN.phl[3805]";
connectAttr "D_:TankControl_rotateY.o" "D_RN.phl[3806]";
connectAttr "D_:TankControl_rotateZ.o" "D_RN.phl[3807]";
connectAttr "D_:TankControl_translateX.o" "D_RN.phl[3808]";
connectAttr "D_:TankControl_translateY.o" "D_RN.phl[3809]";
connectAttr "D_:TankControl_translateZ.o" "D_RN.phl[3810]";
connectAttr "D_:TankControl_visibility.o" "D_RN.phl[3811]";
connectAttr "D_:L_Clavicle_scaleX.o" "D_RN.phl[3812]";
connectAttr "D_:L_Clavicle_scaleY.o" "D_RN.phl[3813]";
connectAttr "D_:L_Clavicle_scaleZ.o" "D_RN.phl[3814]";
connectAttr "D_:L_Clavicle_translateX.o" "D_RN.phl[3815]";
connectAttr "D_:L_Clavicle_translateY.o" "D_RN.phl[3816]";
connectAttr "D_:L_Clavicle_translateZ.o" "D_RN.phl[3817]";
connectAttr "D_:L_Clavicle_visibility.o" "D_RN.phl[3818]";
connectAttr "D_:R_Clavicle_scaleX.o" "D_RN.phl[3819]";
connectAttr "D_:R_Clavicle_scaleY.o" "D_RN.phl[3820]";
connectAttr "D_:R_Clavicle_scaleZ.o" "D_RN.phl[3821]";
connectAttr "D_:R_Clavicle_translateX.o" "D_RN.phl[3822]";
connectAttr "D_:R_Clavicle_translateY.o" "D_RN.phl[3823]";
connectAttr "D_:R_Clavicle_translateZ.o" "D_RN.phl[3824]";
connectAttr "D_:R_Clavicle_visibility.o" "D_RN.phl[3825]";
connectAttr "D_:HeadControl_Mask.o" "D_RN.phl[3826]";
connectAttr "D_:HeadControl_rotateX.o" "D_RN.phl[3827]";
connectAttr "D_:HeadControl_rotateY.o" "D_RN.phl[3828]";
connectAttr "D_:HeadControl_rotateZ.o" "D_RN.phl[3829]";
connectAttr "D_:HeadControl_visibility.o" "D_RN.phl[3830]";
connectAttr "D_:NeckControl_pointConstraint_nodeState.o" "D_RN.phl[3831]";
connectAttr "D_:NeckControl_pointConstraint_target_0__targetWeight.o" "D_RN.phl[3832]"
		;
connectAttr "D_:NeckControl_pointConstraint_offsetX.o" "D_RN.phl[3833]";
connectAttr "D_:NeckControl_pointConstraint_offsetY.o" "D_RN.phl[3834]";
connectAttr "D_:NeckControl_pointConstraint_offsetZ.o" "D_RN.phl[3835]";
connectAttr "D_:NeckControl_pointConstraint_neckW0.o" "D_RN.phl[3836]";
connectAttr "D_:LShoulderFK_scaleX.o" "D_RN.phl[3837]";
connectAttr "D_:LShoulderFK_scaleY.o" "D_RN.phl[3838]";
connectAttr "D_:LShoulderFK_scaleZ.o" "D_RN.phl[3839]";
connectAttr "D_:LShoulderFK_rotateX.o" "D_RN.phl[3840]";
connectAttr "D_:LShoulderFK_rotateY.o" "D_RN.phl[3841]";
connectAttr "D_:LShoulderFK_rotateZ.o" "D_RN.phl[3842]";
connectAttr "D_:LShoulderFK_visibility.o" "D_RN.phl[3843]";
connectAttr "D_:LElbowFK_rotateX.o" "D_RN.phl[3844]";
connectAttr "D_:LElbowFK_rotateY.o" "D_RN.phl[3845]";
connectAttr "D_:LElbowFK_rotateZ.o" "D_RN.phl[3846]";
connectAttr "D_:LElbowFK_visibility.o" "D_RN.phl[3847]";
connectAttr "D_:L_Wrist_scaleX.o" "D_RN.phl[3848]";
connectAttr "D_:L_Wrist_scaleY.o" "D_RN.phl[3849]";
connectAttr "D_:L_Wrist_scaleZ.o" "D_RN.phl[3850]";
connectAttr "D_:L_Wrist_rotateX.o" "D_RN.phl[3851]";
connectAttr "D_:L_Wrist_rotateY.o" "D_RN.phl[3852]";
connectAttr "D_:L_Wrist_rotateZ.o" "D_RN.phl[3853]";
connectAttr "D_:L_Wrist_visibility.o" "D_RN.phl[3854]";
connectAttr "D_:RShoulderFK_scaleX.o" "D_RN.phl[3855]";
connectAttr "D_:RShoulderFK_scaleY.o" "D_RN.phl[3856]";
connectAttr "D_:RShoulderFK_scaleZ.o" "D_RN.phl[3857]";
connectAttr "D_:RShoulderFK_rotateX.o" "D_RN.phl[3858]";
connectAttr "D_:RShoulderFK_rotateY.o" "D_RN.phl[3859]";
connectAttr "D_:RShoulderFK_rotateZ.o" "D_RN.phl[3860]";
connectAttr "D_:RShoulderFK_visibility.o" "D_RN.phl[3861]";
connectAttr "D_:RElbowFK_scaleX.o" "D_RN.phl[3862]";
connectAttr "D_:RElbowFK_scaleY.o" "D_RN.phl[3863]";
connectAttr "D_:RElbowFK_scaleZ.o" "D_RN.phl[3864]";
connectAttr "D_:RElbowFK_rotateX.o" "D_RN.phl[3865]";
connectAttr "D_:RElbowFK_rotateY.o" "D_RN.phl[3866]";
connectAttr "D_:RElbowFK_rotateZ.o" "D_RN.phl[3867]";
connectAttr "D_:RElbowFK_visibility.o" "D_RN.phl[3868]";
connectAttr "D_:R_Wrist_scaleX.o" "D_RN.phl[3869]";
connectAttr "D_:R_Wrist_scaleY.o" "D_RN.phl[3870]";
connectAttr "D_:R_Wrist_scaleZ.o" "D_RN.phl[3871]";
connectAttr "D_:R_Wrist_rotateX.o" "D_RN.phl[3872]";
connectAttr "D_:R_Wrist_rotateY.o" "D_RN.phl[3873]";
connectAttr "D_:R_Wrist_rotateZ.o" "D_RN.phl[3874]";
connectAttr "D_:R_Wrist_visibility.o" "D_RN.phl[3875]";
connectAttr "D_:HipControl_scaleX.o" "D_RN.phl[3876]";
connectAttr "D_:HipControl_scaleY.o" "D_RN.phl[3877]";
connectAttr "D_:HipControl_scaleZ.o" "D_RN.phl[3878]";
connectAttr "D_:HipControl_rotateX.o" "D_RN.phl[3879]";
connectAttr "D_:HipControl_rotateY.o" "D_RN.phl[3880]";
connectAttr "D_:HipControl_rotateZ.o" "D_RN.phl[3881]";
connectAttr "D_:HipControl_visibility.o" "D_RN.phl[3882]";
connectAttr "D_:HipControl_pointConstraint_nodeState.o" "D_RN.phl[3883]";
connectAttr "D_:HipControl_pointConstraint_target_0__targetWeight.o" "D_RN.phl[3884]"
		;
connectAttr "D_:HipControl_pointConstraint_offsetX.o" "D_RN.phl[3885]";
connectAttr "D_:HipControl_pointConstraint_offsetY.o" "D_RN.phl[3886]";
connectAttr "D_:HipControl_pointConstraint_offsetZ.o" "D_RN.phl[3887]";
connectAttr "D_:HipControl_pointConstraint_hipW0.o" "D_RN.phl[3888]";
connectAttr "D_RN.phl[3889]" "pairBlend3.irx2";
connectAttr "D_RN.phl[3890]" "pairBlend4.irx2";
connectAttr "layer1.di" "pPlane1.do";
connectAttr "polyPlane1.out" "pPlaneShape1.i";
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
connectAttr "unitConversion1.o" "D_RN.phl[1963]";
connectAttr "LFoot.o" "D_RN.phl[2014]";
connectAttr "unitConversion2.o" "D_RN.phl[2073]";
connectAttr "RFoot.o" "D_RN.phl[2124]";
connectAttr "D_:RElbowFK_scaleX1.o" "D_RN.phl[3459]";
connectAttr "D_:RElbowFK_scaleY1.o" "D_RN.phl[3460]";
connectAttr "D_:RElbowFK_scaleZ1.o" "D_RN.phl[3461]";
connectAttr "D_:RElbowFK_rotateX1.o" "D_RN.phl[3462]";
connectAttr "D_:RElbowFK_rotateY1.o" "D_RN.phl[3463]";
connectAttr "D_:RElbowFK_rotateZ1.o" "D_RN.phl[3464]";
connectAttr "D_:RElbowFK_visibility1.o" "D_RN.phl[3465]";
connectAttr "layerManager.dli[1]" "layer1.id";
connectAttr "D_RN.phl[2015]" "pairBlend1.w";
connectAttr "pairBlend1_inTranslateY1.o" "pairBlend1.ity1";
connectAttr "D_RN.phl[2125]" "pairBlend2.w";
connectAttr "pairBlend2_inTranslateY1.o" "pairBlend2.ity1";
connectAttr "D_RN.phl[1964]" "pairBlend3.w";
connectAttr "pairBlend3_inRotateX1.o" "pairBlend3.irx1";
connectAttr "D_RN.phl[2074]" "pairBlend4.w";
connectAttr "pairBlend4_inRotateX1.o" "pairBlend4.irx1";
connectAttr "lightLinker1.msg" ":lightList1.ln" -na;
connectAttr "pPlaneShape1.iog" ":initialShadingGroup.dsm" -na;
// End of diver_hitreact2.ma
