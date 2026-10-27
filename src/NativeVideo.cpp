#include "NativeVideo.h"
#include "Mpris.h"
#include "TrackChoice.h"
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <QPointer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QScreen>
#include <QTimer>
#include <QDebug>

struct MpvState {
    mpv_handle *handle=nullptr;
    void *x11Display=nullptr,*waylandDisplay=nullptr;
    std::atomic_bool renderReady{false};
    ~MpvState(){if(handle)mpv_terminate_destroy(handle);}
};
static QVariant valueOf(const mpv_node &node) {
    switch(node.format){
    case MPV_FORMAT_STRING:return QString::fromUtf8(node.u.string?node.u.string:"");
    case MPV_FORMAT_FLAG:return bool(node.u.flag);
    case MPV_FORMAT_INT64:return qlonglong(node.u.int64);
    case MPV_FORMAT_DOUBLE:return node.u.double_;
    case MPV_FORMAT_NODE_ARRAY:{QVariantList list;for(int i=0;i<node.u.list->num;++i)list<<valueOf(node.u.list->values[i]);return list;}
    case MPV_FORMAT_NODE_MAP:{QVariantMap map;for(int i=0;i<node.u.list->num;++i)map[QString::fromUtf8(node.u.list->keys[i])]=valueOf(node.u.list->values[i]);return map;}
    default:return {};
    }
}
static QVariant get(mpv_handle *handle,const char *name){mpv_node node{};if(mpv_get_property(handle,name,MPV_FORMAT_NODE,&node)<0)return {};auto value=valueOf(node);mpv_free_node_contents(&node);return value;}

class VideoRenderer : public QQuickFramebufferObject::Renderer {
public:
    VideoRenderer(NativeVideo *item,std::shared_ptr<MpvState> state):m_item(item),m_state(std::move(state)){}
    ~VideoRenderer()override{m_state->renderReady=false;if(m_context){mpv_render_context_set_update_callback(m_context,nullptr,nullptr);mpv_render_context_free(m_context);}}
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size)override{
        QOpenGLFramebufferObjectFormat format;format.setInternalTextureFormat(GL_RGBA8);format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size,format);
    }
    void render()override{
        if(!m_state->handle)return;
        QQuickOpenGLUtils::resetOpenGLState();
        // Qt Quick's texture uploads leave pixel-store state outside its reset helper.
        // libmpv uploads YUV planes assuming the OpenGL defaults.
        auto glState=QOpenGLContext::currentContext()->extraFunctions();
        glState->glPixelStorei(GL_UNPACK_ALIGNMENT,4);
        glState->glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glState->glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        glState->glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
        glState->glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        if(!m_context){
            mpv_opengl_init_params gl{};
            gl.get_proc_address=[](void *,const char *name)->void*{auto context=QOpenGLContext::currentContext();return context?reinterpret_cast<void*>(context->getProcAddress(QByteArray(name))):nullptr;};
            QList<mpv_render_param> params{{MPV_RENDER_PARAM_API_TYPE,const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,&gl}};
            if(m_state->x11Display)params.append({MPV_RENDER_PARAM_X11_DISPLAY,m_state->x11Display});
            if(m_state->waylandDisplay)params.append({MPV_RENDER_PARAM_WL_DISPLAY,m_state->waylandDisplay});
            params.append({MPV_RENDER_PARAM_INVALID,nullptr});
            const int result=mpv_render_context_create(&m_context,m_state->handle,params.data());
            if(result<0){qWarning()<<"No puedo crear la superficie de vídeo:"<<mpv_error_string(result);return;}
            mpv_render_context_set_update_callback(m_context,[](void *opaque){auto renderer=static_cast<VideoRenderer*>(opaque);if(renderer->m_item)QMetaObject::invokeMethod(renderer->m_item,"update",Qt::QueuedConnection);},this);
            m_state->renderReady=true;if(m_item)QMetaObject::invokeMethod(m_item,"rendererReady",Qt::QueuedConnection);
        }
        auto fbo=framebufferObject();
        mpv_opengl_fbo target{int(fbo->handle()),fbo->width(),fbo->height(),GL_RGBA8};int flip=0;
        mpv_render_param params[]{{MPV_RENDER_PARAM_OPENGL_FBO,&target},{MPV_RENDER_PARAM_FLIP_Y,&flip},{MPV_RENDER_PARAM_INVALID,nullptr}};
        mpv_render_context_update(m_context);mpv_render_context_render(m_context,params);
        QQuickOpenGLUtils::resetOpenGLState();
        if(m_item)QMetaObject::invokeMethod(m_item,"reportFrame",Qt::QueuedConnection);
    }
private:
    QPointer<NativeVideo> m_item;
    std::shared_ptr<MpvState> m_state;
    mpv_render_context *m_context=nullptr;
};

NativeVideo::NativeVideo(QQuickItem *parent):QQuickFramebufferObject(parent),m_state(std::make_shared<MpvState>()){
    setMirrorVertically(false);
#if QT_CONFIG(xcb)
    if(auto native=qGuiApp->nativeInterface<QNativeInterface::QX11Application>())m_state->x11Display=native->display();
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
#if QT_CONFIG(wayland)
    if(auto native=qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())m_state->waylandDisplay=native->display();
#endif
#endif
    new Mpris(this);
    m_state->handle=mpv_create();
    if(!m_state->handle){m_error="No puedo iniciar el motor de vídeo.";return;}
    const auto handle=m_state->handle;
    for(const auto &option:QList<QPair<QByteArray,QByteArray>>{{"config","no"},{"vo","libmpv"},{"hwdec","auto-safe"},{"terminal","no"},{"input-default-bindings","no"},{"input-vo-keyboard","no"},{"osc","no"},{"idle","yes"},{"keep-open","yes"},{"sub-auto","fuzzy"},{"audio-file-auto","no"},{"ytdl","no"},{"volume","80"},{"dither-depth","8"}})
        mpv_set_option_string(handle,option.first.constData(),option.second.constData());
    if(qEnvironmentVariableIsSet("ARCA_TEST_AUDIO_NULL")){mpv_set_option_string(handle,"ao","null");mpv_set_option_string(handle,"hwdec","no");m_hardware=false;}
    int result=mpv_initialize(handle);
    if(result<0){m_error=QString::fromUtf8(mpv_error_string(result));return;}
    mpv_request_log_messages(handle,"error");
    for(const auto &name:{"time-pos","duration","pause","volume","speed","aid","sid","track-list","eof-reached","sub-delay","sub-scale"})mpv_observe_property(handle,0,name,MPV_FORMAT_NONE);
    mpv_set_wakeup_callback(handle,[](void *opaque){QMetaObject::invokeMethod(static_cast<NativeVideo*>(opaque),"processEvents",Qt::QueuedConnection);},this);
}
NativeVideo::~NativeVideo(){if(m_state->handle)mpv_set_wakeup_callback(m_state->handle,nullptr,nullptr);}
QQuickFramebufferObject::Renderer *NativeVideo::createRenderer()const{return new VideoRenderer(const_cast<NativeVideo*>(this),m_state);}
void NativeVideo::reportFrame(){if(m_status==LoadedMedia||m_status==BufferedMedia){++m_frames;emit frameRendered();}}
void NativeVideo::command(const QStringList &args){if(!m_state->handle)return;QList<QByteArray> bytes;for(const auto &arg:args)bytes<<arg.toUtf8();QList<const char*> argv;for(const auto &arg:bytes)argv<<arg.constData();argv<<nullptr;mpv_command_async(m_state->handle,0,argv.data());}
void NativeVideo::property(const char *name,const QVariant &value){if(!m_state->handle)return;if(value.metaType().id()==QMetaType::Bool){int flag=value.toBool();mpv_set_property_async(m_state->handle,0,name,MPV_FORMAT_FLAG,&flag);}else if(value.metaType().id()==QMetaType::Double){double number=value.toDouble();mpv_set_property_async(m_state->handle,0,name,MPV_FORMAT_DOUBLE,&number);}else{auto text=value.toString().toUtf8();char *data=text.data();mpv_set_property_async(m_state->handle,0,name,MPV_FORMAT_STRING,&data);}}
void NativeVideo::setStatus(MediaStatus status){if(m_status==status)return;m_status=status;emit mediaStatusChanged();emit playbackStateChanged();}
void NativeVideo::setSource(const QUrl &url){if(m_source==url)return;m_source=url;m_retried=false;m_error.clear();m_decodeLog.clear();m_position=0;m_duration=0;m_frames=0;emit sourceChanged();emit positionChanged();emit durationChanged();emit errorChanged();load();}
void NativeVideo::load(qint64 at){if(m_source.isEmpty())return;if(!m_source.isLocalFile()){m_error="Selecciona un archivo local.";emit errorChanged();return;}setStatus(LoadingMedia);m_pendingStart=at;if(!m_state->renderReady){update();return;}auto position=QByteArray::number(at/1000.0);mpv_set_property_string(m_state->handle,"start",position.constData());command({"loadfile",m_source.toLocalFile(),"replace"});}
void NativeVideo::rendererReady(){if(!m_source.isEmpty()&&m_status==LoadingMedia)load(m_pendingStart);}
QSize NativeVideo::videoSize()const{auto parameters=get(m_state->handle,"video-params").toMap();return QSize(parameters.value("w").toInt(),parameters.value("h").toInt());}
void NativeVideo::play(){if(m_status==NoMedia&&!m_source.isEmpty())load();if(m_status==EndOfMedia){setStatus(LoadedMedia);setPosition(0);}property("pause",false);m_paused=false;emit playbackStateChanged();}
void NativeVideo::pause(){property("pause",true);m_paused=true;emit playbackStateChanged();}
void NativeVideo::stop(){command({"stop"});setStatus(NoMedia);}
void NativeVideo::setPosition(qint64 milliseconds){if(m_status==EndOfMedia&&milliseconds<m_duration)setStatus(LoadedMedia);command({"seek",QString::number(qMax<qint64>(0,milliseconds)/1000.0,'f',3),"absolute+exact"});}
void NativeVideo::setVolume(double value){value=qBound(0.0,value,1.0);if(qFuzzyCompare(value,m_volume))return;m_volume=value;property("volume",value*100);emit volumeChanged();}
void NativeVideo::setPlaybackRate(double value){value=qBound(0.25,value,4.0);if(qFuzzyCompare(value,m_rate))return;m_rate=value;property("speed",value);emit playbackRateChanged();}
void NativeVideo::setHardware(bool enabled){if(m_hardware==enabled)return;m_hardware=enabled;property("hwdec",enabled?"auto-safe":"no");emit hardwareChanged();}
void NativeVideo::setSubtitleDelay(double seconds){if(qFuzzyCompare(seconds,m_delay))return;m_delay=seconds;property("sub-delay",seconds);emit subtitleDelayChanged();}
void NativeVideo::setSubtitleScale(double scale){scale=qBound(0.5,scale,2.0);if(qFuzzyCompare(scale,m_scale))return;m_scale=scale;property("sub-scale",scale);emit subtitleScaleChanged();}
void NativeVideo::setFitMode(int mode){mode=qBound(0,mode,2);m_fit=mode;property("keepaspect",mode!=2);property("panscan",mode==1?1.0:0.0);emit fitModeChanged();}
void NativeVideo::addSubtitle(const QUrl &url){if(url.isLocalFile())command({"sub-add",url.toLocalFile(),"select"});}
int NativeVideo::activeAudioTrack()const{for(int i=0;i<m_audio.size();++i)if(m_audio[i].toMap().value("nativeId").toInt()==m_aid)return i;return -1;}
int NativeVideo::activeSubtitleTrack()const{for(int i=0;i<m_subs.size();++i)if(m_subs[i].toMap().value("nativeId").toInt()==m_sid)return i;return -1;}
void NativeVideo::setActiveAudioTrack(int index){if(index>=0&&index<m_audio.size())property("aid",m_audio[index].toMap().value("nativeId"));}
void NativeVideo::setActiveSubtitleTrack(int index){property("sid",index>=0&&index<m_subs.size()?m_subs[index].toMap().value("nativeId"):QVariant("no"));}
void NativeVideo::retrySoftware(){const auto at=m_position;m_error.clear();emit errorChanged();m_retried=true;setHardware(false);load(at);}
QStringList NativeVideo::screens()const{QStringList names;for(auto screen:QGuiApplication::screens())names<<screen->name();return names;}
void NativeVideo::moveToScreen(int index){auto screens=QGuiApplication::screens();if(window()&&index>=0&&index<screens.size()){window()->setScreen(screens[index]);window()->setPosition(screens[index]->geometry().topLeft());window()->showFullScreen();}}
void NativeVideo::refreshTracks(){
    m_audio.clear();m_subs.clear();
    for(const auto &value:get(m_state->handle,"track-list").toList()){
        auto native=value.toMap();const auto type=native.value("type").toString();if(type!="audio"&&type!="sub")continue;
        const bool external=native.value("external").toBool();const auto filename=native.value("external-filename").toString();
        QVariantMap track{{"nativeId",native.value("id")},{"id",external?"ext:"+QFileInfo(filename).fileName():native.value("ff-index",native.value("id")).toString()},
                          {"lang",TrackChoice::language(native.value("lang"))},{"forced",native.value("forced").toBool()},{"external",external},{"title",native.value("title")},{"channels",native.value("demux-channel-count")},{"codec",native.value("codec")},{"url",external?QUrl::fromLocalFile(filename):QUrl()},{"name",QFileInfo(filename).fileName()}};
        if(type=="audio")m_audio<<track;else m_subs<<track;
    }
    const auto aid=get(m_state->handle,"aid"),sid=get(m_state->handle,"sid");m_aid=aid.toInt();m_sid=sid.toInt();emit tracksChanged();
}
void NativeVideo::processEvents(){
    if(!m_state->handle)return;
    while(auto event=mpv_wait_event(m_state->handle,0)){
        if(event->event_id==MPV_EVENT_NONE)break;
        if(event->event_id==MPV_EVENT_LOG_MESSAGE){auto log=static_cast<mpv_event_log_message*>(event->data);m_decodeLog=(m_decodeLog+QString::fromUtf8(log->text)).right(600);}
        else if(event->event_id==MPV_EVENT_FILE_LOADED){m_duration=qRound64(get(m_state->handle,"duration").toDouble()*1000);emit durationChanged();refreshTracks();setStatus(LoadedMedia);}
        else if(event->event_id==MPV_EVENT_END_FILE){auto end=static_cast<mpv_event_end_file*>(event->data);if(end->reason==MPV_END_FILE_REASON_ERROR){if(m_hardware&&!m_retried){retrySoftware();continue;}m_error=QString::fromUtf8(mpv_error_string(end->error));if(!m_decodeLog.trimmed().isEmpty())m_error+="\n"+m_decodeLog.trimmed();emit errorChanged();}else if(end->reason==MPV_END_FILE_REASON_EOF&&m_status!=LoadingMedia)setStatus(EndOfMedia);}
        else if(event->event_id==MPV_EVENT_PROPERTY_CHANGE){auto change=static_cast<mpv_event_property*>(event->data);const QByteArray name(change->name);
            if(name=="time-pos"){m_position=qRound64(get(m_state->handle,"time-pos").toDouble()*1000);emit positionChanged();}
            else if(name=="duration"){m_duration=qRound64(get(m_state->handle,"duration").toDouble()*1000);emit durationChanged();}
            else if(name=="pause"){m_paused=get(m_state->handle,"pause").toBool();emit playbackStateChanged();}
            else if(name=="track-list"||name=="aid"||name=="sid")refreshTracks();
            else if(name=="eof-reached"&&m_status!=LoadingMedia&&m_status!=NoMedia&&get(m_state->handle,"eof-reached").toBool())setStatus(EndOfMedia);
            else if(name=="volume"){m_volume=get(m_state->handle,"volume").toDouble()/100;emit volumeChanged();}
            else if(name=="speed"){m_rate=get(m_state->handle,"speed").toDouble();emit playbackRateChanged();}
            else if(name=="sub-delay"){m_delay=get(m_state->handle,"sub-delay").toDouble();emit subtitleDelayChanged();}
            else if(name=="sub-scale"){m_scale=get(m_state->handle,"sub-scale").toDouble();emit subtitleScaleChanged();}
        }
    }
}
