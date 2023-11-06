/*
 * Copyright (c) 2023
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __VOP_Switch_h__
#define __VOP_Switch_h__

#include <VOP/VOP_Node.h>

/// @file
/// This example contains code which shows how to perform some of the most
/// common tasks when writing a custom VEX code operator. It produces code
/// which selects one of the inputs to feed to the output.
namespace HDK_Sample {

/// C++ VOP node to select one of its inputs and feed it into the output.
class VOP_Switch : public VOP_Node
{
public:
    /// Creates an instance of this node with the given name in the given network.
    static OP_Node		*myConstructor(OP_Network *net,
					       const char *name,
					       OP_Operator *entry);

    /// Our parameter templates.
    static PRM_Template		 myTemplateList[];

    /// Disable our parameters based on which inputs are connected.
    bool	 updateParmsFlags() override;

    /// Generate the code for this operator.
    void	 getCode(UT_String &codestr,
                         const VOP_CodeGenContext &context) override;

    /// Provides the labels to appear on input and output buttons.
    const char	*inputLabel(unsigned idx) const override;
    const char	*outputLabel(unsigned idx) const override;

    /// Controls the number of input buttons visible on the node tile.
    unsigned	 getNumVisibleInputs() const override;

    /// Returns the index of the first "unordered" input, meaning that inputs
    /// before this one should not be collapsed when they are disconnected.
    unsigned	 orderedInputs() const override;

protected:
    VOP_Switch(OP_Network *net, const char *name, OP_Operator *entry);
    ~VOP_Switch() override;

    /// Returns the internal name of the supplied input. This input name is
    /// used when the code generator substitutes variables return by getCode.
    void	 getInputNameSubclass(UT_String &in, int idx) const override;
    /// Reverse mapping of internal input names to an input index.
    int		 getInputFromNameSubclass(const UT_String &in) const override;
    /// Fills in the info about the vop type connected to the idx-th input.
    void	 getInputTypeInfoSubclass(VOP_TypeInfo &type_info, 
					  int idx) override;

    /// Returns the internal name of the supplied output. This name is used
    /// when the code generator substitutes variables return by getCode.
    void	 getOutputNameSubclass(UT_String &out, int idx) const override;
    /// Fills out the info about the data type of each output connector.
    void	 getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, 
					   int idx) override;

    /// This methods should be overriden in most VOP nodes. It should fill
    /// in voptypes vector with vop types that are allowed to be connected
    /// to this node at the input at index idx. Note that 
    /// this method should return valid vop types even when nothing is 
    /// connected to the corresponding input.
    void	 getAllowedInputTypeInfosSubclass(unsigned idx,
				    VOP_VopTypeInfoArray &type_infos) override;

    /// Accesses the parameter that controls how out of bounds switch indices
    /// are handled.
    int		OUTOFBOUNDS();
};

}	// End of HDK_Sample namespace

#endif
