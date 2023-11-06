

#include "rop_test.h"

#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <SOP/SOP_Node.h>

#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <CH/CH_LocalVariable.h>

#include <UT/UT_DSOVersion.h>
#include <UT/UT_OFStream.h>
#include <UT/UT_String.h>
#include <UT/UT_StringHolder.h>

#include <GU/GU_Detail.h>
#include <GEO/GEO_PrimPoly.h>
#include <OP/OP_Director.h>
#include <OP/OP_Node.h>
#include <SOP/SOP_Node.h>
#include <iostream>

namespace ork::hfs {

static int* _ifdIndirect = nullptr;

///////////////////////////////////////////////////////////////////////////////

template <> UT_String DumpNodes::fetchParameter(const UT_String& key, fpreal t) {
    // A convenience method to evaluate our custom file parameter.
	int idx = 0;
	int vi = 0;
	UT_String out_value;
    this->evalString(out_value, key, &_ifdIndirect[idx], vi, t);
	return out_value;
}

///////////////////////////////////////////////////////////////////////////////

DumpNodes::DumpNodes(OP_Network* net, const char* name, OP_Operator* entry)
    : ROP_Node(net, name, entry) {

  if (!_ifdIndirect)
    _ifdIndirect = allocIndirect(16);
}

///////////////////////////////////////////////////////////////////////////////

DumpNodes::~DumpNodes() {
}

///////////////////////////////////////////////////////////////////////////////

static void printBoxPolygons(std::ostream& os, OP_Node* node, fpreal t) {
  // Cast the generic OP_Node to a SOP_Node to work with geometry
  SOP_Node* sopNode = node->castToSOPNode();
  if (sopNode) {
    // Create an OP_Context with the desired evaluation time
    OP_Context context(t);

    // Cook the SOP to make sure it's up to date
    if (!sopNode->cook(context)) {
      std::cerr << "Failed to cook SOP Node." << std::endl;
      return;
    }

    // Get the detail (geometry) from the SOP node
    const GU_Detail* gdp = sopNode->getCookedGeo(context);
    if (gdp) {
      // Iterate over the primitives (polygons)
      os << "------------------\n";
      for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it) {
        const GEO_Primitive* prim = gdp->getGEOPrimitive(it.getOffset());

        // Check if this primitive is a polygon
        if (prim->getTypeId() == GEO_PRIMPOLY) {
          const GEO_PrimPoly* poly = static_cast<const GEO_PrimPoly*>(prim);

          // Print some info about the polygon
          os << "\t\tPolygon with " << poly->getVertexCount() << " vertices. " << std::endl;

          // You can iterate over the vertices of the polygon if needed
          // ...
        }
      }
      os << "------------------\n";
    } else {
      std::cerr << "Failed to get geometry from SOP Node." << std::endl;
    }
  } else {
    // std::cerr << "Node is not a SOP Node." << std::endl;
  }
}

///////////////////////////////////////////////////////////////////////////////

static void printNode(std::ostream& os, OP_Node* node, int indent) {
  UT_WorkBuffer wbuf;
  wbuf.sprintf("%*s", indent * 2, "");

  if (const OP_Operator* op = node->getOperator()) {
    // Retrieve the operator type name and table name
    auto typeName  = UT_String(op->getName());
    auto tableName = UT_String(op->getTableName());

    //bool typ_valid = (typeName.findWord("Invalid") == nullptr);
    bool tbl_valid = (tableName.findWord("Invalid") == nullptr);

    if (not tbl_valid) {
      tableName = "---";
    }

    os << wbuf.buffer() << node->getName() << " : typ<" << typeName << "> tbl<" << tableName << ">\n";

    printBoxPolygons(os, node, 0.0);
  } else {
    os << wbuf.buffer() << node->getName() << "\n";
  }

  for (int i = 0; i < node->getNchildren(); ++i)
    printNode(os, node->getChild(i), indent + 2);
}

///////////////////////////////////////////////////////////////////////////////

int DumpNodes::startRender(int /*nframes*/, fpreal tstart, fpreal tend) {
  _startTime = tstart;
  _endTime = tend;
  if (error() < UT_ERROR_ABORT)
    executePreRenderScript(tstart);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////

ROP_RENDER_CODE
DumpNodes::renderFrame(fpreal time, UT_Interrupt*) {
  
  executePreFrameScript(time); // Execute the pre-render script.

  ///////////////////////////////////////////
  // Evaluate the parameter for the file name
  //  and write something to the file.
  ///////////////////////////////////////////

  auto filename = fetchParameter<UT_String>("file", time);

  UT_OFStream os(filename);
  printNode(os, OPgetDirector(), 0);
  os.close();

  ///////////////////////////////////////////

  if (error() < UT_ERROR_ABORT)  // Execute the post-render script.
    executePostFrameScript(time);

  return ROP_CONTINUE_RENDER;
}

///////////////////////////////////////////////////////////////////////////////

ROP_RENDER_CODE
DumpNodes::endRender() {
  if (error() < UT_ERROR_ABORT)
    executePostRenderScript(_endTime);
  return ROP_CONTINUE_RENDER;
}

///////////////////////////////////////////////////////////////////////////////
// parameter definitions / template for UI
///////////////////////////////////////////////////////////////////////////////

static PRM_Name filename("file", "Save to file");
static PRM_Default default_value(0, "output.txt");

//

static PRM_Template* getTemplates() {

  static PRM_Template* template_owned_by_hfs = 0;

  if (template_owned_by_hfs)
    return template_owned_by_hfs;

  template_owned_by_hfs     = new PRM_Template[14];
  template_owned_by_hfs[0]  = PRM_Template(PRM_FILE, 1, &filename, &default_value);
  template_owned_by_hfs[1]  = theRopTemplates[ROP_TPRERENDER_TPLATE];
  template_owned_by_hfs[2]  = theRopTemplates[ROP_PRERENDER_TPLATE];
  template_owned_by_hfs[3]  = theRopTemplates[ROP_LPRERENDER_TPLATE];
  template_owned_by_hfs[4]  = theRopTemplates[ROP_TPREFRAME_TPLATE];
  template_owned_by_hfs[5]  = theRopTemplates[ROP_PREFRAME_TPLATE];
  template_owned_by_hfs[6]  = theRopTemplates[ROP_LPREFRAME_TPLATE];
  template_owned_by_hfs[7]  = theRopTemplates[ROP_TPOSTFRAME_TPLATE];
  template_owned_by_hfs[8]  = theRopTemplates[ROP_POSTFRAME_TPLATE];
  template_owned_by_hfs[9]  = theRopTemplates[ROP_LPOSTFRAME_TPLATE];
  template_owned_by_hfs[10] = theRopTemplates[ROP_TPOSTRENDER_TPLATE];
  template_owned_by_hfs[11] = theRopTemplates[ROP_POSTRENDER_TPLATE];
  template_owned_by_hfs[12] = theRopTemplates[ROP_LPOSTRENDER_TPLATE];
  template_owned_by_hfs[13] = PRM_Template();

  return template_owned_by_hfs;
}

//

static OP_TemplatePair* __getTemplatePair() {
  // a singleton I guess
  static OP_TemplatePair* the_template_pair = nullptr;
  if (!the_template_pair) {
    OP_TemplatePair* base;

    base    = new OP_TemplatePair(getTemplates());
    the_template_pair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
  }
  return the_template_pair;
}

//

static OP_VariablePair* __getVariablePair() {
  static OP_VariablePair* pair = 0;
  if (!pair)
    pair = new OP_VariablePair(ROP_Node::myVariableList);
  return pair;
}

///////////////////////////////////////////////////////////////////////////////

OP_Node* __factory(OP_Network* net, const char* name, OP_Operator* op) {
  return new DumpNodes(net, name, op);
}

} // namespace ork::hfs

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// register with houdini..
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void newDriverOperator(OP_OperatorTable* op_table) {

  auto the_op = new OP_Operator(
      "rop_test",
      "Dump Tree",
      ork::hfs::__factory,
      ork::hfs::__getTemplatePair(),
      0,
      0,
      ork::hfs::__getVariablePair(),
      OP_FLAG_GENERATOR);

  op_table->addOperator(the_op);
}