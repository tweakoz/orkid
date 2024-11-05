# Orkid has a built in PEG Parser.

## It can take a scanner grammar (example):

```python
    macro(M1)           <| "xyz" |>
    MULTI_LINE_COMMENT  <| "\/\*([^*]|\*+[^/*])*\*+\/" |>
    SINGLE_LINE_COMMENT <| "\/\/.*[\n\r]" |>
    WHITESPACE          <| "\s+" |>
    NEWLINE             <| "[\n\r]+" |>
    EQUALS              <| "=" |>
    COMMA               <| "," |>
    COLON               <| ":" |>
    SEMICOLON           <| ";" |>
    L_SQUARE            <| "\[" |>
    R_SQUARE            <| "\]" |>
    L_PAREN             <| "\(" |>
    R_PAREN             <| "\)" |>
    L_CURLY             <| "\{" |>
    R_CURLY             <| "\}" |>
    DOT                 <| "\." |>
    STAR                <| "\*" |>
    PLUS                <| "\+" |>
    MINUS               <| "\-" |>
    INTEGER             <| "-?(\d+)" |>
    FLOATING_POINT      <| "-?(\d*\.?)(\d+)([eE][-+]?\d+)?" |>
    FUNCTION            <| "function" |>
    KW_FLOAT            <| "float" |>
    KW_INT              <| "int" |>
    KW_VEC2             <| "vec2" |>
    KW_VEC3             <| "vec3" |>
    KW_VEC4             <| "vec4" |>
    KW_MAT2             <| "mat2" |>
    KW_MAT3             <| "mat3" |>
    KW_MAT4             <| "mat4" |>
    KW_VTXSHADER        <| "vertex_shader" |>
    KW_FRGSHADER        <| "fragment_shader" |>
    KW_COMSHADER        <| "compute_shader" |>
    KW_UNISET           <| "uniform_set" |>
    KW_UNIBLK           <| "uniform_block" |>
    KW_VTXIFACE         <| "vertex_interface" |>
    KW_FRGIFACE         <| "fragment_interface" |>
    KW_INPUTS           <| "inputs" |>
    KW_OUTPUTS          <| "outputs" |>
    KW_SAMP1D           <| "sampler1D" |>
    KW_SAMP2D           <| "sampler2D" |>
    KW_SAMP3D           <| "sampler3D" |>
    KW_OR_ID            <| "[a-zA-Z_][a-zA-Z0-9_]*" |>
```

## A parser grammar (example):

```python
    datatype       <| sel{ KW_FLOAT KW_INT 
                           KW_VEC2 KW_VEC3 KW_VEC4 
                           KW_MAT2 KW_MAT3 KW_MAT4 
                           KW_SAMP1D KW_SAMP2D KW_SAMP3D } |>
    number         <| sel{INTEGER FLOATING_POINT} |>
    kw_or_id       <| KW_OR_ID |>
    dot            <| DOT |>
    l_square       <| L_SQUARE |>
    r_square       <| R_SQUARE |>
    l_paren        <| L_PAREN |>
    r_paren        <| R_PAREN |>
    plus           <| PLUS |>
    minus          <| MINUS |>
    star           <| STAR |>
    l_curly        <| L_CURLY |>
    r_curly        <| R_CURLY |>
    semicolon      <| SEMICOLON |>
    colon          <| COLON |>
    equals         <| EQUALS |>
    kw_function    <| FUNCTION |>
    kw_vtxshader   <| KW_VTXSHADER |>
    kw_frgshader   <| KW_FRGSHADER |>
    kw_comshader   <| KW_COMSHADER |>
    kw_uniset      <| KW_UNISET |>
    kw_uniblk      <| KW_UNIBLK |>
    kw_vtxiface    <| KW_VTXIFACE |>
    kw_frgiface    <| KW_FRGIFACE |>
    kw_inputs      <| KW_INPUTS |>
    kw_outputs     <| KW_OUTPUTS |>

    member_ref     <| [ dot kw_or_id ] |>
    array_ref      <| [ l_square expression r_square ] |>

    object_subref  <| sel{ member_ref array_ref } |>

    inh_list_item  <| [ colon kw_or_id ] |>
    inh_list       <| zom{ inh_list_item } |>

    fn_arg         <| [ expression opt{COMMA} ] |>
    fn_args        <| zom{ fn_arg } |>

    fn_invok <| [
        [ kw_or_id ] : "fni_name"
        l_paren
        fn_args
        r_paren
    ] |>

    product <| [ primary opt{ [star primary] } ] |>

    sum <| sel{
        [ product plus product ] : "add"
        [ product minus product ] : "sub"
        product : "pro"
    } |>

    expression <| [ sum ] |>

    term <| [ l_paren expression r_paren ] |>

    typed_identifier <| [datatype kw_or_id] |>

    primary <| sel{ fn_invok
                    number
                    term
                    [ kw_or_id zom{object_subref} ] : "primary_var_ref"
                  } |>

    assignment_statement <| [
        sel { 
          [ typed_identifier ] : "astatement_vardecl"
          [ kw_or_id ] : "astatement_varref"
        }
        equals
        expression
    ] |>

    statement <| sel{ 
        [ assignment_statement semicolon ]
        [ fn_invok semicolon ]
        semicolon
    } |>

    arg_list <| zom{ [ typed_identifier opt{COMMA} ] } |>
    statement_list <| zom{ statement } |>

    fn_def <| [
        kw_function
        [ kw_or_id ] : "fn_name"
        l_paren
        arg_list : "args"
        r_paren
        l_curly
        statement_list : "fn_statements"
        r_curly
    ] |>
    
    vtx_shader <| [
        kw_vtxshader
        [ kw_or_id ] : "vtx_name"
        zom{inh_list_item} : "vtx_dependencies"
        l_curly
        statement_list : "vtx_statements"
        r_curly
    ] |>

    frg_shader <| [
        kw_frgshader
        [ kw_or_id ] : "frg_name"
        zom{ inh_list_item } : "frg_dependencies"
        l_curly
        statement_list : "frg_statements"
        r_curly
    ] |>

    com_shader <| [
        kw_comshader
        [ kw_or_id ] : "com_name"
        zom{ inh_list_item } : "com_dependencies"
        l_curly
        statement_list : "com_statements"
        r_curly
    ] |>

    data_decl <| [typed_identifier semicolon] |>

    data_decls <| zom{ data_decl } |>

    uniset <| [
      kw_uniset
      [ kw_or_id ] : "uniset_name"
      l_curly
      data_decls : "uniset_decls"
      r_curly
    ] |>

    uniblk <| [
      kw_uniblk
      [ kw_or_id ] : "uniblk_name"
      l_curly
      data_decls : "uniblk_decls"
      r_curly
    ] |>

    iface_input <| [ typed_identifier opt{ [colon kw_or_id] } semicolon ] |>

    iface_inputs <| [
      kw_inputs
      l_curly
      zom{ iface_input } : "inputlist"
      r_curly
    ] |>

    iface_outputs <| [
      kw_outputs
      l_curly
      zom{ 
        [ data_decl ] : "output_decl"
      }
      r_curly
    ] |>

    vtx_iface <| [
      kw_vtxiface
      [ kw_or_id ] : "vif_name"
      zom{ inh_list_item } : "vif_dependencies"
      l_curly
      iface_inputs
      iface_outputs
      r_curly
    ] |>

    frg_iface <| [
      kw_frgiface
      [ kw_or_id ] : "fif_name"
      zom{ inh_list_item } : "fif_dependencies"
      l_curly
      iface_inputs
      iface_outputs
      r_curly
    ] |>

    translatable <| sel{ fn_def vtx_shader frg_shader com_shader uniset uniblk vtx_iface frg_iface } |>

    translation_unit <| zom{ translatable } |>
```

## A string or file to parse (example):
```glsl
        ///////////////////////////////////////////////////////////////
        uniform_set ublock_vtx { 
          mat4 mvp;
          mat4 mvp_l;
          mat4 mvp_r;
        }
        ///////////////////////////////////////////////////////////////
        uniform_block ublock_frg {
          vec4 ModColor;
          sampler2D ColorMap;
        }
        ///////////////////////////////////////////////////////////////
        vertex_interface iface_vdefault : ublock_vtx {
          inputs {
            vec4 position : POSITION;
            vec4 vtxcolor : COLOR0;
            vec2 uv0 : TEXCOORD0;
            vec2 uv1 : TEXCOORD1;
          }
          outputs {
            vec4 frg_clr;
            vec2 frg_uv;
          }
        }
        ///////////////////////////////////////////////////////////////
        fragment_interface iface_fdefault {
          inputs {
            vec4 frg_clr;
            vec2 frg_uv;
          }
          outputs {
            vec4 out_clr;
          }
        }
        ///////////////////////////////////////////////////////////////
        fragment_interface iface_fmt : ublock_frg {
          inputs {
            vec2 frg_uv;
          }
          outputs {
            vec4 out_clr;
          }
        }       
        ///////////////////////////////////////////////////////////////
        function abc(int x, float y) {
            float a = 1.0;
            float v = 2.0;
            float b = (x+y)*7.0;
            v = v*2.0;
        }
        ///////////////////////////////////////////////////////////////
        vertex_shader vs_uitext : iface_vdefault {
          gl_Position = mvp * position;
          frg_clr     = vtxcolor;
          frg_uv      = uv0;
        }
        ///////////////////////////////////////////////////////////////
        fragment_shader ps_uitext : iface_fmt {
          vec4 s = texture(ColorMap, frg_uv);
          float texa = pow(s.a*s.r,0.75);
          //out_clr = vec4(ModColor.xyz, texa*ModColor.w);
        }
        ///////////////////////////////////////////////////////////////
        compute_shader cu_xxx {
          vec4 s = myFunction(ColorMap, frg_uv);
          float texa = pow(object.param.A[3],0.75);
          //out_clr = vec4(ModColor.xyz, texa*ModColor.w);
        }
        ///////////////////////////////////////////////////////////////
        function def() {
            float X = (1.0+2.3)*7.0;
        }
```

## And generate an AST tree (example):

```python
TranslationUnit()
  UniformSet(ublock_vtx)
    ObjectName(ublock_vtx)
    DataDeclarations
      DataDeclaration dt<mat4> id<mvp>
      DataDeclaration dt<mat4> id<mvp_l>
      DataDeclaration dt<mat4> id<mvp_r>
  UniformBlk(ublock_frg)
    ObjectName(ublock_frg)
    DataDeclarations
      DataDeclaration dt<vec4> id<ModColor>
      DataDeclaration dt<sampler2D> id<ColorMap>
  VertexInterface(iface_vdefault)
    ObjectName(iface_vdefault)
    Dependency(ublock_vtx)
    InterfaceInputs
      InterfaceInput id<position> sem<POSITION> dt<vec4>
      InterfaceInput id<vtxcolor> sem<COLOR0> dt<vec4>
      InterfaceInput id<uv0> sem<TEXCOORD0> dt<vec2>
      InterfaceInput id<uv1> sem<TEXCOORD1> dt<vec2>
    InterfaceOutputs
      InterfaceOutput id<frg_clr> dt<vec4>
      InterfaceOutput id<frg_uv> dt<vec2>
  FragmentInterface(iface_fdefault)
    ObjectName(iface_fdefault)
    InterfaceInputs
      InterfaceInput id<frg_clr> sem<> dt<vec4>
      InterfaceInput id<frg_uv> sem<> dt<vec2>
    InterfaceOutputs
      InterfaceOutput id<out_clr> dt<vec4>
  FragmentInterface(iface_fmt)
    ObjectName(iface_fmt)
    Dependency(ublock_frg)
    InterfaceInputs
      InterfaceInput id<frg_uv> sem<> dt<vec2>
    InterfaceOutputs
      InterfaceOutput id<out_clr> dt<vec4>
  FunctionDef(abc)
    ObjectName(abc)
    ArgumentList()
      TypedIdentifier dt<int> id<x>
      TypedIdentifier dt<float> id<y>
    StatementList()
      AssignmentStatement
        AssignmentStatementVarDecl dt<float> id<a>
        Expression
          Sum(_)
            product
              primary
                FloatLiteral(1.0)
      AssignmentStatement
        AssignmentStatementVarDecl dt<float> id<v>
        Expression
          Sum(_)
            product
              primary
                FloatLiteral(2.0)
      AssignmentStatement
        AssignmentStatementVarDecl dt<float> id<b>
        Expression
          Sum(_)
            product
              primary
                Term
                  Expression
                    Sum(+)
                      product
                        primary
                          ObjectName(x)
                      product
                        primary
                          ObjectName(y)
              primary
                FloatLiteral(7.0)
      AssignmentStatement
        AssignmentStatementVarRef
        Expression
          Sum(_)
            product
              primary
                ObjectName(v)
              primary
                FloatLiteral(2.0)
  VertexShader(vs_uitext)
    ObjectName(vs_uitext)
    Dependency(iface_vdefault)
    StatementList()
      AssignmentStatement
        AssignmentStatementVarRef
        Expression
          Sum(_)
            product
              primary
                ObjectName(mvp)
              primary
                ObjectName(position)
      AssignmentStatement
        AssignmentStatementVarRef
        Expression
          Sum(_)
            product
              primary
                ObjectName(vtxcolor)
      AssignmentStatement
        AssignmentStatementVarRef
        Expression
          Sum(_)
            product
              primary
                ObjectName(uv0)
  FragmentShader(ps_uitext)
    ObjectName(ps_uitext)
    Dependency(iface_fmt)
    StatementList()
      AssignmentStatement
        AssignmentStatementVarDecl dt<vec4> id<s>
        Expression
          Sum(_)
            product
              primary
                FunctionInvokation()
                  ObjectName(texture)
                  FunctionInvokationArguments()
                    FunctionInvokationArgument()
                      Expression
                        Sum(_)
                          product
                            primary
                              ObjectName(ColorMap)
                    FunctionInvokationArgument()
                      Expression
                        Sum(_)
                          product
                            primary
                              ObjectName(frg_uv)
      AssignmentStatement
        AssignmentStatementVarDecl dt<float> id<texa>
        Expression
          Sum(_)
            product
              primary
                FunctionInvokation()
                  ObjectName(pow)
                  FunctionInvokationArguments()
                    FunctionInvokationArgument()
                      Expression
                        Sum(_)
                          product
                            primary
                              ObjectName(s)
                                MemberRef(a)
                            primary
                              ObjectName(s)
                                MemberRef(r)
                    FunctionInvokationArgument()
                      Expression
                        Sum(_)
                          product
                            primary
                              FloatLiteral(0.75)
  ComputeShader(cu_xxx)
    ObjectName(cu_xxx)
    StatementList()
      AssignmentStatement
        AssignmentStatementVarDecl dt<vec4> id<s>
        Expression
          Sum(_)
            product
              primary
                FunctionInvokation()
                  ObjectName(myFunction)
                  FunctionInvokationArguments()
                    FunctionInvokationArgument()
                      Expression
                        Sum(_)
                          product
                            primary
                              ObjectName(ColorMap)
                    FunctionInvokationArgument()
                      Expression
                        Sum(_)
                          product
                            primary
                              ObjectName(frg_uv)
      AssignmentStatement
        AssignmentStatementVarDecl dt<float> id<texa>
        Expression
          Sum(_)
            product
              primary
                FunctionInvokation()
                  ObjectName(pow)
                  FunctionInvokationArguments()
                    FunctionInvokationArgument()
                      Expression
                        Sum(_)
                          product
                            primary
                              ObjectName(object)
                                MemberRef(param)
                                MemberRef(A)
                                ArrayRef
                                  Expression
                                    Sum(_)
                                      product
                                        primary
                                          IntegerLiteral(3)
                    FunctionInvokationArgument()
                      Expression
                        Sum(_)
                          product
                            primary
                              FloatLiteral(0.75)
  FunctionDef(def)
    ObjectName(def)
    ArgumentList()
    StatementList()
      AssignmentStatement
        AssignmentStatementVarDecl dt<float> id<X>
        Expression
          Sum(_)
            product
              primary
                Term
                  Expression
                    Sum(+)
                      product
                        primary
                          FloatLiteral(1.0)
                      product
                        primary
                          FloatLiteral(2.3)
              primary
                FloatLiteral(7.0)
```

A more complex AST (From the shader language parser)

![ParticleShaderAST:1](particle.dot.png)

