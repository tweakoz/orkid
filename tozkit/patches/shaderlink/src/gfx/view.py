##################################################################################
#
#  Shaderlink - A RenderMan Shader Authoring Toolkit 
#  http://libe.ocracy.org/shaderlink.html
#  2010 Libero Spagnolini (libero.spagnolini@gmail.com)
#
##################################################################################

import math
import sys

from PyQt4 import QtCore, QtGui

class GfxNodeBuilder(object):   
   def __init__(self, node):
       self.node = node
       
       self.headerFont = QtGui.QFont('Calibri', 15)
       self.headerFont.setBold(True)
       self.headerFont.setItalic(True)
       self.propertyFont = QtGui.QFont('Calibri', 13)
       
       # pens
       self.borderPen = QtGui.QPen(QtGui.QBrush(QtGui.QColor(225, 223, 193)), 
                                   1.0, 
                                   QtCore.Qt.SolidLine,
                                   QtCore.Qt.RoundCap,
                                   QtCore.Qt.RoundJoin)
       self.propPen = QtGui.QPen(QtGui.QBrush(QtGui.QColor(202, 223, 155)), 
                                 1.0, 
                                 QtCore.Qt.SolidLine,
                                 QtCore.Qt.RoundCap,
                                 QtCore.Qt.RoundJoin)
       self.headerPen = QtGui.QPen(QtGui.QBrush(QtGui.QColor(225, 223, 193)), 
                                   1.0, 
                                   QtCore.Qt.SolidLine,
                                   QtCore.Qt.RoundCap,
                                   QtCore.Qt.RoundJoin)
       
       # brushes       
       self.rectBrush = QtGui.QBrush(QtGui.QColor(63, 63, 63))     
       self.headerBrush = QtGui.QBrush(QtGui.QColor(88, 118, 119))
       self.headerSelectedBrush = QtGui.QBrush(QtGui.QColor(150, 58, 70))
       self.shadowBrush = QtGui.QBrush(QtGui.QColor(50, 50, 50))
       self.propsBrushes = {'C' : QtGui.QBrush(QtGui.QColor(QtCore.Qt.yellow)), 
                            'F' : QtGui.QBrush(QtGui.QColor(QtCore.Qt.red)), 
                            'M' : QtGui.QBrush(QtGui.QColor(QtCore.Qt.green)), 
                            'P' : QtGui.QBrush(QtGui.QColor(QtCore.Qt.gray)), 
                            'S' : QtGui.QBrush(QtGui.QColor(QtCore.Qt.magenta)), 
                            'V' : QtGui.QBrush(QtGui.QColor(QtCore.Qt.cyan)),
                            'N' : QtGui.QBrush(QtGui.QColor(QtCore.Qt.cyan))}
       
              
       # tweak separators
       self.inputOutputSeparator = 50
       self.edgeSeparator = 3  
       self.headerSeparator = 6
       
       # node dimensions
       self.computeGfxNodeSizes()
       
       # properties dimensions
       self.computeGfxNodePropsSizes()
       
   def computeGfxNodeSizes(self):
        # compute heights of header and properties
        headerFontMetric = QtGui.QFontMetricsF(self.headerFont)
        propFontMetric = QtGui.QFontMetricsF(self.propertyFont)
        
        headerFontHeight = headerFontMetric.height()
        propFontHeight = propFontMetric.height()
        
        # find the largest width input and output text
        # min is 10
        widthsInput = [10]
        for prop in self.node.inputProps:
            widthsInput.append(propFontMetric.width(prop.name)) 

        widthsOutput = [10]
        for prop in self.node.outputProps:
            widthsOutput.append(propFontMetric.width(prop.name)) 
        
        maxInputWidth = max(widthsInput)
        maxOutputWidth = max(widthsOutput) 
        
        # little offset and connectionBox dimensions
        propBoxSize = propFontHeight * 0.75
        
        # compute total height and width
        totalHeight = headerFontHeight + propBoxSize
        totalHeight += max([len(self.node.inputProps), len(self.node.outputProps)]) * 2 * propBoxSize
        
        totalWidth = self.edgeSeparator + propBoxSize  
        totalWidth += propBoxSize * 0.5 + maxInputWidth  
        totalWidth += propBoxSize * 0.5 + self.inputOutputSeparator + propBoxSize * 0.5
        totalWidth += maxOutputWidth + propBoxSize * 0.5 
        totalWidth += propBoxSize + self.edgeSeparator  

        totalWidthHeader = self.edgeSeparator + propBoxSize  
        totalWidthHeader += propBoxSize * 0.5 + headerFontMetric.width(self.node.name) +  propBoxSize * 0.5 
        totalWidthHeader += propBoxSize + self.edgeSeparator  
        
        # clamp to header width
        totalWidth = max([totalWidthHeader, totalWidth])
        
        # preview size
        self.previewPixmapSizeStartP = (10, totalHeight + 10)
        self.previewPixmapWidth = totalWidth - 20 
        self.previewPixmapHeight = totalWidth - 20
        if self.node.gfxNode: # TODO: refactor
            if self.node.gfxNode.previewPixmapEnabled:
                totalHeight += totalWidth             
            
        # store computed stuff
        self.rect = QtCore.QRectF(0, 0, totalWidth, totalHeight)
        self.shadowRect = QtCore.QRectF(5, 5, totalWidth, totalHeight)
        self.propBoxSize = propBoxSize
        self.headerFontHeight = headerFontHeight
        self.headerHeight = propBoxSize
        self.maxInputWidth = maxInputWidth
        self.maxOutputWidth = maxOutputWidth   
        
   def computeGfxNodePropsSizes(self):
        # input properties
        self.inputPropsDim = {}
        startInputPropBoxP = QtCore.QPointF(self.rect.x() + self.edgeSeparator, 
                                            self.rect.y() + self.propBoxSize + self.headerHeight) 
        
        for i in range(len(self.node.inputProps)):
            startPoint = QtCore.QPointF(startInputPropBoxP.x(), 
                                        startInputPropBoxP.y() + 2 * i * self.propBoxSize)         
            box = QtCore.QRectF(startPoint.x(), startPoint.y(), 
                                self.propBoxSize, self.propBoxSize)
            textPoint = box.bottomRight() + QtCore.QPointF(0.5 * self.propBoxSize, 0.0) 
            self.inputPropsDim[self.node.inputProps[i]] = {'box' : box, 'point' : textPoint}      
        
        # output properties
        self.outputPropsDim = {}
        startOutputPropP = QtCore.QPointF(self.rect.topRight().x() - self.propBoxSize - self.edgeSeparator, 
                                          self.rect.y() + self.propBoxSize + self.headerHeight)    
        
        for i in range(len(self.node.outputProps)):
            startPoint = QtCore.QPointF(startOutputPropP.x(), 
                                        startOutputPropP.y() + 2 * i * self.propBoxSize)           
            box = QtCore.QRectF(startPoint.x(), startPoint.y(), 
                                self.propBoxSize, self.propBoxSize)
            textPoint = box.bottomLeft() + QtCore.QPointF(- 0.5 * self.propBoxSize - self.maxOutputWidth, 0.0)        
            self.outputPropsDim[self.node.outputProps[i]] = {'box' : box, 'point' : textPoint}                        

class GfxNode(QtGui.QGraphicsItem):
    Type = QtGui.QGraphicsItem.UserType + 1    
    
    def __init__(self, node):
        QtGui.QGraphicsItem.__init__(self)        
        
        # flag (new from QT 4.6...)
        self.setFlag(QtGui.QGraphicsItem.ItemSendsScenePositionChanges)
        self.setFlag(QtGui.QGraphicsItem.ItemSendsGeometryChanges)
        
        # wrapped node
        self.node = node
                        
        # qt graphics stuff
        self.setFlag(QtGui.QGraphicsItem.ItemIsMovable)
        self.setFlag(QtGui.QGraphicsItem.ItemIsSelectable)
        self.setZValue(10)           
        
        # preview img
        self.previewPixmap = None
        self.previewPixmapEnabled = False
                
        # builder for graphics settings
        self.gfxNodeBuilder = GfxNodeBuilder(self.node)
        
        # selection flag
        self.isNodeSelected = False
        
        # item rect
        self.rect = self.gfxNodeBuilder.rect.united(self.gfxNodeBuilder.shadowRect)
        
        # store pixmap for preview
        self.pixmap, self.scaledPixmap = self.buildPixmapPreview()   
                
    def allOutputGfxLinks(self):
        gfxLinks = []
        for outputLinks in self.node.outputLinks.values():
            for outputLink in outputLinks:
                gfxLinks.append(outputLink.gfxLink)
            
        return gfxLinks

    def allInputGfxLinks(self):
        gfxLinks = []
        for inputLink in self.node.inputLinks.values():
                gfxLinks.append(inputLink.gfxLink)

        return gfxLinks
    
    def pointFromProperty(self, prop):
        from core.model import Property
        # if we look for an input property
        if prop.category == Property.Input:
            gSettings = self.gfxNodeBuilder.inputPropsDim[prop]
            p = gSettings['box'].center() - QtCore.QPointF(self.gfxNodeBuilder.edgeSeparator + gSettings['box'].width() * 0.575, 
                                                           0.0) 
            return self.mapToScene(p) 
        else:
            gSettings = self.gfxNodeBuilder.outputPropsDim[prop]
            p = gSettings['box'].center() + QtCore.QPointF(self.gfxNodeBuilder.edgeSeparator + gSettings['box'].width() * 0.575, 
                                                           0.0) 
            return self.mapToScene(p) 
                            
    def propertyAt(self, point):
        pickData = {}
        itemSpaceP = self.mapFromScene(point)
        
        # loop on input properties
        for prop, gSettings in self.gfxNodeBuilder.inputPropsDim.iteritems():
            if gSettings['box'].contains(itemSpaceP):
                pickData['property'] = prop
                p = gSettings['box'].center() - QtCore.QPointF(self.gfxNodeBuilder.edgeSeparator + gSettings['box'].width() * 0.575, 
                                                               0.0) 
                pickData['point'] = self.mapToScene(p) 
                return pickData

        # loop on output properties
        for prop, gSettings in self.gfxNodeBuilder.outputPropsDim.iteritems(): 
            if gSettings['box'].contains(itemSpaceP):
                pickData['property'] = prop
                p = gSettings['box'].center() + QtCore.QPointF(self.gfxNodeBuilder.edgeSeparator + gSettings['box'].width() * 0.575, 
                                                               0.0) 
                pickData['point'] = self.mapToScene(p)  
                return pickData
        
        return None

    def drawGfxNode(self, painter):
        painter.setRenderHint(QtGui.QPainter.Antialiasing)
        painter.setRenderHint(QtGui.QPainter.SmoothPixmapTransform)
        
        # shadow
        painter.setPen(QtCore.Qt.NoPen)        
        painter.setBrush(self.gfxNodeBuilder.shadowBrush)
        painter.drawRect(self.gfxNodeBuilder.shadowRect)    
                
        # whole node        
        painter.setPen(self.gfxNodeBuilder.borderPen)
        painter.setBrush(self.gfxNodeBuilder.rectBrush)
        painter.drawRect(self.gfxNodeBuilder.rect)        
        
        # header
        if self.isNodeSelected:
            painter.setBrush(self.gfxNodeBuilder.headerSelectedBrush)
        else:
            painter.setBrush(self.gfxNodeBuilder.headerBrush)
        painter.drawRect(QtCore.QRectF(self.gfxNodeBuilder.rect.topLeft(), 
                                       self.gfxNodeBuilder.rect.topRight() + QtCore.QPointF(0, 25)))
        painter.setFont(self.gfxNodeBuilder.headerFont)
        painter.setPen(self.gfxNodeBuilder.headerPen)
        painter.drawText(QtCore.QPointF(self.gfxNodeBuilder.rect.x() + 10, 
                                        self.gfxNodeBuilder.rect.y() + 20), self.node.name)
                
        # input properties
        painter.setPen(self.gfxNodeBuilder.propPen)        
        painter.setFont(self.gfxNodeBuilder.propertyFont)
        
        for prop, gSettings in self.gfxNodeBuilder.inputPropsDim.iteritems():        
            painter.setBrush(self.gfxNodeBuilder.propsBrushes[prop.encodedStr()])
            painter.drawRect(gSettings['box']) 
            painter.drawText(gSettings['point'], prop.name) 

        # output properties
        painter.setPen(self.gfxNodeBuilder.propPen) 
        painter.setFont(self.gfxNodeBuilder.propertyFont)
        
        for prop, gSettings in self.gfxNodeBuilder.outputPropsDim.iteritems():        
            painter.setBrush(self.gfxNodeBuilder.propsBrushes[prop.encodedStr()])
            painter.drawRect(gSettings['box']) 
            painter.drawText(gSettings['point'], prop.name) 
            
        # preview pixmap
        if self.previewPixmapEnabled:
            painter.drawPixmap(self.gfxNodeBuilder.previewPixmapSizeStartP[0],
                               self.gfxNodeBuilder.previewPixmapSizeStartP[1],
                               self.gfxNodeBuilder.previewPixmapWidth,
                               self.gfxNodeBuilder.previewPixmapHeight,
                               self.previewPixmap)
        
    def paint(self, painter, option, widget):        
        self.drawGfxNode(painter)

    def buildPixmapPreview(self):
        pixmap = QtGui.QPixmap(self.rect.width(), self.rect.height())
        pixmap.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter()
        painter.begin(pixmap)
        self.drawGfxNode(painter)
        painter.end()
        
        scaledSize = QtCore.QSize(int(self.rect.width() * 0.75), int(self.rect.height() * 0.75))
        scaledPixmap = pixmap.scaled(scaledSize, QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation)
        return pixmap, scaledPixmap
                
    def type(self):
        return GfxNode.Type

    def boundingRect(self):        
        return self.rect

    def shape(self):
        shape = QtGui.QPainterPath()               
        shape.addRect(self.rect)

        return shape  
    
    def itemChange(self, change, value):
        if change == QtGui.QGraphicsItem.ItemSelectedChange:
            self.isNodeSelected = not self.isNodeSelected
        elif change == QtGui.QGraphicsItem.ItemPositionHasChanged:
            # invalidate all the links attached
            for inputLink in self.node.inputLinks.values():
                    inputLink.gfxLink.adjust(True)

            for outputLinks in self.node.outputLinks.values():
                for outputLink in outputLinks:
                    outputLink.gfxLink.adjust(True)
        
        return QtGui.QGraphicsItem.itemChange(self, change, value)
    
    def setPreviewPixmap(self, pixmap):
        if not pixmap or pixmap.isNull():
            self.previewPixmap = None
            self.previewPixmapEnabled = False
        else:
            self.previewPixmap = pixmap
            self.previewPixmapEnabled = True
        
        # build sizes
        self.gfxNodeBuilder.computeGfxNodeSizes()
        
        # item rect
        self.prepareGeometryChange()
        self.rect = self.gfxNodeBuilder.rect.united(self.gfxNodeBuilder.shadowRect)
        
        # schedules a redraw of the area covered
        self.update()

class GfxLink(QtGui.QGraphicsItem):
    Type = QtGui.QGraphicsItem.UserType + 2    
    
    @staticmethod
    def createFromPoints(sourceP, destP):
        gfxLink = GfxLink()
        
        # convert to item space
        gfxLink.sourcePoint = gfxLink.mapToItem(gfxLink, sourceP)
        gfxLink.destPoint = gfxLink.mapToItem(gfxLink, destP)
        
        # adjust link 
        gfxLink.adjust()
        
        return gfxLink
                
    @staticmethod
    def createFromLink(link):
        gfxLink = GfxLink()
        
        # wrapped link
        gfxLink.link = link
        
        # path
        gfxLink.path = QtGui.QPainterPath()            
        
        # adjust link 
        gfxLink.adjust(True)
        
        return gfxLink
    
    def __init__(self):
        QtGui.QGraphicsItem.__init__(self)

        # qt graphics stuff
        self.selectedBrush = QtGui.QBrush(QtGui.QColor(150, 58, 70))
        self.brush = QtGui.QBrush(QtGui.QColor(225, 223, 193)) 
        self.setFlag(QtGui.QGraphicsItem.ItemIsSelectable)
        self.setZValue(5)           
        
        # item rect
        self.rect = None
        
        # bezier points
        self.points = []
        
        # selection flag
        self.isLinkSelected = False

    def setSourcePoint(self, p):
        self.sourcePoint = self.mapToItem(self, p)
        
        # adjust link 
        self.adjust()

    def setDestinationPoint(self, p):
        self.destPoint = self.mapToItem(self, p)
            
        # adjust link 
        self.adjust()

    def adjust(self, computeFromGfxNodes = False):
        if computeFromGfxNodes:
            sourceP = self.link.sourceNode.gfxNode.pointFromProperty(self.link.sourceProp)
            self.sourcePoint = self.mapToItem(self, sourceP)
            destP = self.link.destNode.gfxNode.pointFromProperty(self.link.destProp)
            self.destPoint = self.mapToItem(self, destP)
        
        # clear bezier points
        self.points = []
            
        self.prepareGeometryChange()
                
        # hull spline
        hull = QtCore.QRectF(self.sourcePoint, self.destPoint)
        centerX = hull.center().x()
        centerY = hull.center().y()

        # first 
        self.points.append(self.sourcePoint)
        
        # second point
        offsetVX = abs((hull.topRight().x() - hull.topLeft().x()) * 0.35)
        offsetVY = 0.0
        
        secondP = self.sourcePoint + QtCore.QPointF(offsetVX, offsetVY)
        self.points.append(secondP)
        
        # third point
        thirdPX =  centerX
        thirdPY = self.sourcePoint.y()
        self.points.append(QtCore.QPointF(thirdPX, thirdPY))
        
        # fourth point
        self.points.append(QtCore.QPointF(centerX, centerY))
        
        # fifth point (bezier tangent)
        self.points.append(QtCore.QPointF(centerX, centerY))

        # sixth point
        sixthPX =  centerX
        sixthPY = self.destPoint.y()
        self.points.append(QtCore.QPointF(sixthPX, sixthPY))
        
        # seventh point
        seventhP = self.destPoint - QtCore.QPointF(offsetVX, offsetVY)
        self.points.append(seventhP)
        
        # last
        self.points.append(self.destPoint)
        
        # bezier curve path
        self.path = QtGui.QPainterPath()
        self.path.moveTo(self.points[0])
        self.path.cubicTo(self.points[1], self.points[2], self.points[3])
        self.path.cubicTo(self.points[5], self.points[6], self.points[7])
        
        # arrow
        arrowSize = 3
        if hull.topRight() == self.destPoint:
            arrowAnchor = hull.topRight()
        else:
            arrowAnchor = hull.bottomRight()
            
        firstArrowP = arrowAnchor 
        secondArrowP = QtCore.QPointF(arrowAnchor.x() - arrowSize, arrowAnchor.y() - arrowSize)
        thirdArrowP = QtCore.QPointF(arrowAnchor.x() - arrowSize, arrowAnchor.y() + arrowSize)
        fourthArrowP = arrowAnchor
        
        arrow = QtGui.QPolygonF([firstArrowP, secondArrowP, thirdArrowP, fourthArrowP])
        self.path.addPolygon(arrow)
        
        # rect
        self.rect = self.path.boundingRect()
                
    def paint(self, painter, option, widget):        
        if self.isLinkSelected:        
            painter.setPen(QtGui.QPen(self.selectedBrush, 
                                      1.75, 
                                      QtCore.Qt.SolidLine,
                                      QtCore.Qt.RoundCap,
                                      QtCore.Qt.RoundJoin))
        else:
           painter.setPen(QtGui.QPen(self.brush, 
                                     1.75, 
                                     QtCore.Qt.SolidLine,
                                     QtCore.Qt.RoundCap,
                                     QtCore.Qt.RoundJoin))    
        
        painter.drawPath(self.path)
                        
    def type(self):
        return GfxNode.Type

    def boundingRect(self):        
        return self.path.boundingRect()

    def shape(self):
        return self.path  
    
    def itemChange(self, change, value):
        if change == QtGui.QGraphicsItem.ItemSelectedChange:
            self.isLinkSelected = not self.isLinkSelected
        
        return QtGui.QGraphicsItem.itemChange(self, change, value)    
        
class GfxPanel(QtGui.QGraphicsView):
    def __init__(self, shaderLink, commandProcessor, parent = None):
        QtGui.QGraphicsView.__init__(self, parent)

        # controller
        from controller import GfxPanelController
        self.controller = GfxPanelController(self, 
                                             shaderLink,
                                             commandProcessor)        
        # set scene
        scene = QtGui.QGraphicsScene(self)
        scene.setSceneRect(-10000, -10000, 20000, 20000)
        scene.setItemIndexMethod(QtGui.QGraphicsScene.NoIndex)
        self.setScene(scene)
        
        # qt graphics stuff
        self.setCacheMode(QtGui.QGraphicsView.CacheBackground)
        self.setRenderHint(QtGui.QPainter.Antialiasing)
        self.setTransformationAnchor(QtGui.QGraphicsView.AnchorUnderMouse)
        self.setResizeAnchor(QtGui.QGraphicsView.AnchorViewCenter)
        self.setDragMode(QtGui.QGraphicsView.RubberBandDrag)
        self.setMouseTracking(False)

        self.viewBrush = QtGui.QBrush(QtGui.QColor(92, 102, 115))  
        self.setBackgroundBrush(self.viewBrush)
        
        # connect signals
        self.connect(shaderLink, QtCore.SIGNAL('nodeAdded'), 
                     self.controller.onNodeAdded)

        self.connect(shaderLink, QtCore.SIGNAL('linkAdded'), 
                     self.controller.onLinkAdded)

        self.connect(shaderLink, QtCore.SIGNAL('nodeRemoved'), 
                     self.controller.onNodeRemoved)

        self.connect(shaderLink, QtCore.SIGNAL('linkRemoved'), 
                     self.controller.onLinkRemoved)

    def keyPressEvent(self, event):
        self.controller.onKeyPressEvent(event)
                        
    def wheelEvent(self, event):
        self.controller.onWheelEvent(event)

    def mousePressEvent(self, event):
        self.controller.onMousePressEvent(event)        
        
    def mouseDoubleClickEvent(self, event):
        self.controller.onMouseDoubleClickEvent(event)
        
    def mouseMoveEvent(self, event):
        self.controller.onMouseMoveEvent(event)        
    
    def mouseReleaseEvent(self, event):        
        self.controller.onMouseReleaseEvent(event)  

    def dragEnterEvent(self, event):
        self.controller.onDragEnterEvent(event)

    def dragMoveEvent(self, event):
        self.controller.onDragMoveEvent(event)

    def dropEvent(self, event):
        self.controller.onDropEvent(event)
                        