#include <QDomNode>
#include <QPainter>
#include <QPainterPath>

#include "waveform/renderers/waveformrendermark.h"

#include "controlobject.h"
#include "trackinfoobject.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "widget/wskincolor.h"
#include "widget/wwidget.h"

WaveformRenderMark::WaveformRenderMark( WaveformWidgetRenderer* waveformWidgetRenderer) :
    WaveformRendererAbstract(waveformWidgetRenderer)
{
}

void WaveformRenderMark::init()
{
}

void WaveformRenderMark::setup( const QDomNode& node)
{
    m_marks.clear();
    m_marks.reserve(32);

    QDomNode child = node.firstChild();
    while (!child.isNull())
    {
        if (child.nodeName() == "Mark")
        {
            m_marks.push_back(Mark());
            Mark& mark = m_marks.back();
            setupMark(child,mark);
        }
        child = child.nextSibling();
    }
}


void WaveformRenderMark::draw( QPainter* painter, QPaintEvent* event)
{
    painter->save();

    /*
    //DEBUG
    for( int i = 0; i < m_markPoints.size(); i++)
    {
        if( m_waveformWidget->getTrackSamples())
            painter->drawText(40*i,12+12*(i%3),QString::number(m_markPoints[i]->get() / (double)m_waveformWidget->getTrackSamples()));
    }
    */

    painter->setWorldMatrixEnabled(false);

    for( int i = 0; i < m_marks.size(); i++)
    {
        Mark& mark = m_marks[i];

        if( !mark.m_point)
            continue;

        //Generate pixmap on first paint can't be done in setup since we need
        //render widget to be resized yet ...
        if( mark.m_pixmap.isNull())
            generateMarkPixmap(mark);

        int samplePosition = mark.m_point->get();
        if( samplePosition > 0.0)
        {
            m_waveformRenderer->regulateVisualSample(samplePosition);
            double currentMarkPoint = m_waveformRenderer->transformSampleIndexInRendererWorld(samplePosition);

            //NOTE: vRince I guess pixmap width is odd to display the center on the exact line !
            //external pixmap should respect that ...
            const int markHalfWidth = mark.m_pixmap.width()/2.0;

            //check if the current point need to be displayed
            if( currentMarkPoint > -markHalfWidth && currentMarkPoint < m_waveformRenderer->getWidth() + markHalfWidth)
                painter->drawPixmap(QPoint(currentMarkPoint-markHalfWidth,0), mark.m_pixmap);
        }
    }

    painter->restore();
}

void WaveformRenderMark::setupMark( const QDomNode& node, Mark& mark)
{
    QString item = WWidget::selectNodeQString( node, "Control");
    mark.m_point = ControlObject::getControl( ConfigKey(m_waveformRenderer->getGroup(), item));

    //if there is no control the mark won't be displayed anyway ...
    if( mark.m_point == 0)
        return;

    mark.m_color = WWidget::selectNodeQString( node, "Color");
    if( mark.m_color == "") {

        // As a fallback, grab the mark color from the parent's MarkerColor
        mark.m_color = WWidget::selectNodeQString(node.parentNode(), "MarkerColor");
        qDebug() << "Didn't get mark Color, using parent's MarkerColor:" << mark.m_color;
    }

    mark.m_textColor = WWidget::selectNodeQString(node, "TextColor");
    if( mark.m_textColor == "") {
        // Read the text color, otherwise use the parent's BgColor.
        mark.m_textColor = WWidget::selectNodeQString(node.parentNode(), "BgColor");
        qDebug() << "Didn't get mark TextColor, using parent's BgColor:" << mark.m_textColor;
    }

    //vRince used contains to be able to add horizontal align soon

    QString markAlign = WWidget::selectNodeQString(node, "Align");
    if (markAlign.contains("center", Qt::CaseInsensitive) == 0) {
        mark.m_align = Qt::AlignVCenter;
    } else if (markAlign.contains("bottom", Qt::CaseInsensitive) == 0) {
        mark.m_align = Qt::AlignBottom;
    } else {
        mark.m_align = Qt::AlignTop; // Default
    }

    mark.m_text = WWidget::selectNodeQString(node, "Text");
    mark.m_pixmapPath = WWidget::selectNodeQString(node,"Pixmap");
}

void WaveformRenderMark::generateMarkPixmap( Mark& mark)
{
    // Load the pixmap from file -- takes precedence over text.
    if( mark.m_pixmapPath != "") {
        // TODO(XXX) We could use WPixmapStore here, which would recolor the
        // pixmap according to the theme. Then we would have to worry about
        // deleting it -- for now we'll just load the pixmap directly.
        QString path =  WWidget::getPath(mark.m_pixmapPath);
        mark.m_pixmap = QPixmap( path);

        // If loading the pixmap didn't fail, then we're done. Otherwise fall
        // through and render a label.
        if( !mark.m_pixmap.isNull())
            return;
    }

    QPainter painter;

    int labelRectWidth = 0;
    int labelRectHeight = 0;

    // If no text is provided, leave m_markPixmap as a null pixmap
    if( !mark.m_text.isNull())
    {
        //QFont font("Bitstream Vera Sans");
        //QFont font("Helvetica");
        QFont font; // Uses the application default
        font.setPointSize(8);
        font.setStretch(80);

        QFontMetrics metrics(font);

        //fixed margin ...
        QRect wordRect = metrics.tightBoundingRect(mark.m_text);
        int marginX = 1;
        int marginY = 1;
        wordRect.moveTop(marginX+1);
        wordRect.moveLeft(marginY+1);
        wordRect.setWidth( wordRect.width() + (wordRect.width())%2);
        //even wodrrect to have an event pixmap >> draw the line in the middle !

        labelRectWidth = wordRect.width() + 2*marginX + 1;
        labelRectHeight = wordRect.height() + 2*marginY + 1;

        //vRince all the 0.5 stuff produce nicer rounded rectangle ... I don't know why !
        QRectF labelRect(0.5,0.5,(float)labelRectWidth - 0.5f,(float)labelRectHeight - 0.5f);

        //Vrince TODO
        /*
        switch(mark.m_align)
        {
        case Qt::AlignBottom : labelRect.moveBottom( mark.m_pixmap.height() - 0.5); break;
        case Qt::AlignVCenter : labelRect.moveBottom( (mark.m_pixmap.height() + wordRect.height())/2.0 - 0.5); break; //vRince should we keep that ?
        default : labelRect.moveTop( 0.5); break;
        }
        */

        mark.m_pixmap = QPixmap(labelRectWidth+1, m_waveformRenderer->getHeight());

        // Fill with transparent pixels
        mark.m_pixmap.fill(QColor(0,0,0,0));

        painter.begin(&mark.m_pixmap);
        painter.setRenderHint(QPainter::TextAntialiasing);

        //vRince : Those produce wierd rounded rectangle and produce 2 pixel width vertical line !
        //I prefer no antialiasing ...
        //painter.setRenderHints(QPainter::Antialiasing,true);
        //painter.setRenderHint(QPainter::HighQualityAntialiasing,true);

        painter.setWorldMatrixEnabled(false);

        //draw the label rect
        QColor rectColor = mark.m_color;
        rectColor.setAlpha(50);
        rectColor.darker(100);
        painter.setPen(rectColor);
        painter.setBrush(QBrush(rectColor));
        painter.drawRoundedRect(labelRect, 2.0, 2.0);

        //draw text
        painter.setBrush(QBrush(QColor(0,0,0,0)));
        font.setWeight(40);
        painter.setFont(font);
        painter.setPen(mark.m_textColor);
        painter.drawText(labelRect, Qt::AlignCenter, mark.m_text);

        //draw line
        QColor lineColor = mark.m_color;
        lineColor.setAlpha(100);
        painter.setPen(lineColor);

        float middle = mark.m_pixmap.width()/2;
        float lineTop = labelRectHeight + 1;
        float lineBottom = mark.m_pixmap.height();

        painter.drawLine( middle, lineTop, middle, lineBottom);

        //othe lines to increase contrast
        painter.setPen(QColor(0,0,0,100));
        painter.drawLine( middle - 1, lineTop, middle - 1, lineBottom);
        painter.drawLine( middle + 1, lineTop, middle + 1, lineBottom);

    }
    else //no text draw triangle
    {
        float triangleSize = 9.0;
        mark.m_pixmap = QPixmap(triangleSize+1, m_waveformRenderer->getHeight());
        mark.m_pixmap.fill(QColor(0,0,0,0));

        painter.begin(&mark.m_pixmap);
        painter.setRenderHint(QPainter::TextAntialiasing);

        painter.setWorldMatrixEnabled(false);

        QColor triangleColor = mark.m_color;
        triangleColor.setAlpha(100);
        painter.setPen(QColor(0,0,0,0));
        painter.setBrush(QBrush(triangleColor));

        //vRicne: again don't ask about the +-0.1 0.5 ... just to make it pixel perfect in Qt ...

        QPolygonF triangle;
        triangle.append(QPointF(0.5,0));
        triangle.append(QPointF(triangleSize+0.5,0));
        triangle.append(QPointF(triangleSize*0.5 + 0.1, triangleSize*0.5));

        painter.drawPolygon(triangle);

        triangle.clear();
        triangle.append(QPointF(0.0,mark.m_pixmap.height()));
        triangle.append(QPointF(triangleSize+0.5,mark.m_pixmap.height()));
        triangle.append(QPointF(triangleSize*0.5 + 0.1, mark.m_pixmap.height() - triangleSize*0.5 - 2.1));

        painter.drawPolygon(triangle);

        //TODO vRince duplicated code make a method
        //draw line
        QColor lineColor = mark.m_color;
        lineColor.setAlpha(100);
        painter.setPen(lineColor);
        float middle = mark.m_pixmap.width()/2;

        float lineTop = triangleSize*0.5 + 1;
        float lineBottom = mark.m_pixmap.height() - triangleSize*0.5 - 1;

        painter.drawLine( middle, lineTop, middle, lineBottom);

        //other lines to increase contrast
        painter.setPen(QColor(0,0,0,100));
        painter.drawLine( middle - 1, lineTop, middle - 1, lineBottom);
        painter.drawLine( middle + 1, lineTop, middle + 1, lineBottom);
    }
}
