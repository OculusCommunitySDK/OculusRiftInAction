/************************************************************************************

 Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
 Copyright   :   Copyright Brad Davis. All Rights reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 ************************************************************************************/

#include "Common.h"

#include <QDomDocument>


namespace oria {
  namespace qt {

    template<typename T>
    T toQtType(Resource res) {
      T result;
      size_t size = Resources::getResourceSize(res);
      result.resize(size);
      Resources::getResourceData(res, result.data());
      return result;
    }

    vec2 toGlm(const QSize & size) {
      return vec2(size.width(), size.height());
    }

    vec2 toGlm(const QPointF & pt) {
      return vec2(pt.x(), pt.y());
    }

    QSize sizeFromGlm(const vec2 & size) {
      return QSize(size.x, size.y);
    }

    QPointF pointFromGlm(const vec2 & pt) {
      return QPointF(pt.x, pt.y);
    }

    QByteArray toByteArray(Resource res) {
      return toQtType<QByteArray>(res);
    }

    QString toString(Resource res) {
      QByteArray data = toByteArray(res);
      return QString::fromUtf8(data.data(), data.size());
    }

    QImage loadImageResource(Resource res) {
      QImage image;
      image.loadFromData(toByteArray(res));
      return image;
    }

    QPixmap loadXpmResource(Resource res) {
      QString cursorXpmStr = oria::qt::toString(res);
      QStringList list = cursorXpmStr.split(QRegExp("\\n|\\r\\n|\\r"));
      std::vector<QByteArray> bv;
      std::vector<const char*> v;
      foreach(QString line, list) {
        bv.push_back(line.toLocal8Bit());
        v.push_back(*bv.rbegin());
      }
      QPixmap result = QPixmap(&v[0]);
      return result;
    }
  }
}

typedef std::list<QString> List;
typedef std::map<QString, List> Map;
typedef std::pair<QString, List> Pair;

template <typename F>
void for_each_node(const QDomNodeList & list, F f) {
  for (int i = 0; i < list.size(); ++i) {
    f(list.at(i));
  }
}

static Map createGlslMap() {
  using namespace std;
  Map listMap;
  map<QString, Map> contextMapMap;
  QDomDocument document;
  {
    std::vector<uint8_t> data = Platform::getResourceByteVector(Resource::MISC_GLSL_XML);
    document.setContent(QByteArray((const char*)&data[0], (int)data.size()));
  }
  QDomElement s = document.documentElement().firstChildElement();
  for_each_node(s.childNodes(), [&](QDomNode child) {
    if (QString("list") == child.nodeName()) {
      QString listName = child.attributes().namedItem("name").nodeValue();
      list<QString> & l = listMap[listName];
      for_each_node(child.childNodes(), [&](QDomNode item) {
        if (QString("item") == item.nodeName()) {
          QString nodeValue = item.firstChild().nodeValue();
          l.push_back(item.firstChild().nodeValue());
        }
      });
    }


    if (QString("contexts") == child.nodeName()) {
      for_each_node(child.childNodes(), [&](QDomNode child) {
        if (QString("context") == child.nodeName()) {
          QString contextName = child.attributes().namedItem("name").nodeValue();
          qDebug() << "Context name: " << contextName;
          map<QString, list<QString>> & contextMap = contextMapMap[contextName];
          for_each_node(child.childNodes(), [&](QDomNode child) {
            if (QString("keyword") == child.nodeName()) {
              QString attribute = child.attributes().namedItem("attribute").nodeValue();
              QString value = child.attributes().namedItem("String").nodeValue();
              contextMap[attribute].push_back(value);
            }
          });
        }
      });
    }
  });

  Map finalMap;

  Map contextMap = contextMapMap["v330"];
  std::for_each(contextMap.begin(), contextMap.end(), [&](const Pair & maptype) {
    QString type = maptype.first;
    List & typeList = finalMap[type];
    foreach(const QString & listName, maptype.second) {
      const List & l = listMap[listName];
      typeList.insert(typeList.end(), l.begin(), l.end());
    }
  });

  foreach(const Pair & p, finalMap) {
    qDebug() << p.first;
    foreach(const QString & s, p.second) {
      qDebug() << "\t" << s;
    }
  }

  return finalMap;
}
/*
OffscreenUiWindow::OffscreenUiWindow() {
  // Create a QQuickWindow that is associated with out render control. Note that this
  // window never gets created or shown, meaning that it will never get an underlying
  // native (platform) window.
  m_quickWindow = new QQuickWindow(this);

  // Now hook up the signals. For simplicy we don't differentiate between
  // renderRequested (only render is needed, no sync) and sceneChanged (polish and sync
  // is needed too).
  m_updateTimer.setSingleShot(true);
  m_updateTimer.setInterval(5);
  connect(&m_updateTimer, &QTimer::timeout, this, &OffscreenUiWindow::updateQuick);


  connect(this, &QQuickRenderControl::renderRequested, this, &OffscreenUiWindow::requestUpdate);
  connect(this, &QQuickRenderControl::sceneChanged, this, &OffscreenUiWindow::requestUpdate);
}


void OffscreenUiWindow::requestUpdate() {
  if (!m_updateTimer.isActive())
    m_updateTimer.start();
}

void OffscreenUiWindow::updateQuick() {
  // Polish, synchronize and render the next frame (into our fbo).  In this example
  // everything happens on the same thread and therefore all three steps are performed
  // in succession from here. In a threaded setup the render() call would happen on a
  // separate thread.
  polishItems();
  if (sync()) {
    m_context->makeCurrent(m_surface);
    if (!m_fbo) {
      return;
    }
    m_fbo->bind();
    render();
    m_quickWindow->resetOpenGLState();
    QOpenGLFramebufferObject::bindDefault();
    glFlush();
    GLuint oldTexture = m_currentTexture.exchange(m_fbo->takeTexture());
    if (oldTexture) {
      glDeleteTextures(1, &oldTexture);
    }
//    emit offscreenTextureUpdated();
  }
}


void OffscreenUiWindow::setupScene(const QSize & size, QOpenGLContext * context) {
  m_surface = new QOffscreenSurface();
  m_surface->setFormat(context->format());
  m_surface->create();
  context->makeCurrent(m_surface);

  // Update item and rendering related geometries.
  m_quickWindow->setGeometry(0, 0, size.width(), size.height());
  this->m_context = context;
  // Initialize the render control and our OpenGL resources.
  initialize(context);

  m_fbo = new QOpenGLFramebufferObject(size, QOpenGLFramebufferObject::CombinedDepthStencil);
  m_quickWindow->setRenderTarget(m_fbo);
}

void OffscreenUiWindow::addItem(QQmlComponent* qmlComponent) {
  if (qmlComponent->isError()) {
    QList<QQmlError> errorList = qmlComponent->errors();
    foreach(const QQmlError &error, errorList)
      qWarning() << error.url() << error.line() << error;
  }

  QObject *rootObject = qmlComponent->create();
  if (qmlComponent->isError()) {
    QList<QQmlError> errorList = qmlComponent->errors();
    foreach(const QQmlError &error, errorList)
      qWarning() << error.url() << error.line() << error;
    return;
  }

  QQuickItem *m_rootItem{ nullptr };
  m_rootItem = qobject_cast<QQuickItem *>(rootObject);
  if (!m_rootItem) {
    qWarning("run: Not a QQuickItem");
    delete rootObject;
    return;
  }

  addItem(m_rootItem);
}

void OffscreenUiWindow::addItem(QQuickItem * m_rootItem) {
  // The root item is ready. Associate it with the window.
  m_rootItem->setParentItem(m_quickWindow->contentItem());
}
*/



QOffscreenUi::QOffscreenUi() {
}

QOffscreenUi::~QOffscreenUi() {
  // Make sure the context is current while doing cleanup. Note that we use the
  // offscreen surface here because passing 'this' at this point is not safe: the
  // underlying platform window may already be destroyed. To avoid all the trouble, use
  // another surface that is valid for sure.
  m_context->makeCurrent(m_offscreenSurface);

  // Delete the render control first since it will free the scenegraph resources.
  // Destroy the QQuickWindow only afterwards.
  delete m_renderControl;

  delete m_qmlComponent;
  delete m_quickWindow;
  delete m_qmlEngine;
  delete m_fbo;

  m_context->doneCurrent();

  delete m_offscreenSurface;
  delete m_context;
}



void QOffscreenUi::setup(const QSize & size, QOpenGLContext * shareContext) {
  m_size = size;

  QSurfaceFormat format;
  // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
  format.setDepthBufferSize(16);
  format.setStencilBufferSize(8);
  format.setMajorVersion(3);
  format.setMinorVersion(3);
  format.setProfile(QSurfaceFormat::OpenGLContextProfile::CoreProfile);
  m_context->setFormat(format);
  if (nullptr != shareContext) {
    m_context->setShareContext(shareContext);
  }
  m_context->create();

  // Pass m_context->format(), not format. Format does not specify and color buffer
  // sizes, while the context, that has just been created, reports a format that has
  // these values filled in. Pass this to the offscreen surface to make sure it will be
  // compatible with the context's configuration.
  m_offscreenSurface->setFormat(m_context->format());
  m_offscreenSurface->create();

  m_context->makeCurrent(m_offscreenSurface);
  m_fbo = new QOpenGLFramebufferObject(size, QOpenGLFramebufferObject::CombinedDepthStencil);

  // Create a QQuickWindow that is associated with out render control. Note that this
  // window never gets created or shown, meaning that it will never get an underlying
  // native (platform) window.
  QQuickWindow::setDefaultAlphaBuffer(true);
  m_quickWindow = new QQuickWindow(m_renderControl);
  m_quickWindow->setRenderTarget(m_fbo);

  m_quickWindow->setColor(QColor(255, 255, 255, 0));
  m_quickWindow->setFlags(m_quickWindow->flags() | static_cast<Qt::WindowFlags>(Qt::WA_TranslucentBackground));
  // Create a QML engine.
  m_qmlEngine = new QQmlEngine;
  if (!m_qmlEngine->incubationController())
    m_qmlEngine->setIncubationController(m_quickWindow->incubationController());
  m_qmlEngine->addImageProvider(QLatin1String("resources"), new QResourceImageProvider());

  // When Quick says there is a need to render, we will not render immediately. Instead,
  // a timer with a small interval is used to get better performance.
  m_updateTimer.setSingleShot(true);
  m_updateTimer.setInterval(5);
  connect(&m_updateTimer, &QTimer::timeout, this, &QOffscreenUi::updateQuick);

  // Now hook up the signals. For simplicy we don't differentiate between
  // renderRequested (only render is needed, no sync) and sceneChanged (polish and sync
  // is needed too).
  connect(m_renderControl, &QQuickRenderControl::renderRequested, this, &QOffscreenUi::requestRender);
  connect(m_renderControl, &QQuickRenderControl::sceneChanged, this, &QOffscreenUi::requestUpdate);

  m_qmlComponent = new QQmlComponent(m_qmlEngine);

  // Update item and rendering related geometries.
  m_quickWindow->setGeometry(0, 0, m_size.width(), m_size.height());

  // Initialize the render control and our OpenGL resources.
  m_context->makeCurrent(m_offscreenSurface);
  m_renderControl->initialize(m_context);

}

QQmlContext * QOffscreenUi::qmlContext() {
  if (nullptr == m_rootItem) {
    return m_qmlComponent->creationContext();
  }
  return QQmlEngine::contextForObject(m_rootItem);
}

void QOffscreenUi::loadQml(const QUrl & qmlSource, std::function<void(QQmlContext*)> f) {
  m_qmlComponent->loadUrl(qmlSource);
  if (m_qmlComponent->isLoading())
    connect(m_qmlComponent, &QQmlComponent::statusChanged, this, &QOffscreenUi::run);
  else
    run();
}

void QOffscreenUi::requestUpdate() {
  m_polish = true;
  if (!m_updateTimer.isActive())
    m_updateTimer.start();
}

void QOffscreenUi::requestRender() {
  if (!m_updateTimer.isActive())
    m_updateTimer.start();
}

void QOffscreenUi::run() {
  disconnect(m_qmlComponent, SIGNAL(statusChanged(QQmlComponent::Status)), this, SLOT(run()));
  if (m_qmlComponent->isError()) {
    QList<QQmlError> errorList = m_qmlComponent->errors();
    foreach(const QQmlError &error, errorList)
      qWarning() << error.url() << error.line() << error;
    return;
  }

  QObject *rootObject = m_qmlComponent->create();
  if (m_qmlComponent->isError()) {
    QList<QQmlError> errorList = m_qmlComponent->errors();
    foreach(const QQmlError &error, errorList)
      qWarning() << error.url() << error.line() << error;
    return;
  }

  m_rootItem = qobject_cast<QQuickItem *>(rootObject);
  if (!m_rootItem) {
    qWarning("run: Not a QQuickItem");
    delete rootObject;
    return;
  }

  // The root item is ready. Associate it with the window.
  m_rootItem->setParentItem(m_quickWindow->contentItem());
  m_rootItem->setWidth(m_size.width());
  m_rootItem->setHeight(m_size.height());

  SAY("Finished setting up QML");
}

void QOffscreenUi::updateQuick() {
  if (m_paused) {
    return;
  }
  if (!m_context->makeCurrent(m_offscreenSurface))
    return;

  // Polish, synchronize and render the next frame (into our fbo).  In this example
  // everything happens on the same thread and therefore all three steps are performed
  // in succession from here. In a threaded setup the render() call would happen on a
  // separate thread.
  if (m_polish) {
    m_renderControl->polishItems();
    m_renderControl->sync();
    m_polish = false;
  }

  if (!renderLock.try_lock()) {
    requestRender(); 
    return;
  }
  m_fbo->bind();
  m_renderControl->render();
  renderLock.unlock();
  m_quickWindow->resetOpenGLState();
  QOpenGLFramebufferObject::bindDefault();
  glFlush();

  emit textureUpdated();
}

void QOffscreenUi::deleteOldTextures(const std::vector<GLuint> & oldTextures) {
  if (!m_context->makeCurrent(m_offscreenSurface))
    return;
  m_context->functions()->glDeleteTextures(oldTextures.size(), &oldTextures[0]);
}
