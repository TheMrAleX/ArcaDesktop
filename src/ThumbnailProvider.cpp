#include "ThumbnailProvider.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>
#include <QPainter>
#include <QCryptographicHash>
#include <QDebug>

ThumbnailProvider::ThumbnailProvider(QString directory):QQuickImageProvider(QQuickImageProvider::Image,QQmlImageProviderBase::ForceAsynchronousImageLoading),m_directory(std::move(directory)){QDir().mkpath(m_directory);}
void ThumbnailProvider::setFiles(const QVariantList &files){QHash<QString,QVariantMap> snapshot;for(const auto &value:files){const auto file=value.toMap();auto key=file.value("thumb").toString().section('/',-1);snapshot[key]=file;}QMutexLocker lock(&m_mutex);m_files=std::move(snapshot);}
QImage ThumbnailProvider::requestImage(const QString &id,QSize *size,const QSize &requested){
    const auto key=id.section('?',0,0);QVariantMap file;{QMutexLocker lock(&m_mutex);file=m_files.value(key);}
    if(file.isEmpty()&&qEnvironmentVariableIsSet("ARCA_TEST_AUDIO_NULL"))qWarning()<<"Miniatura desconocida"<<key;
    QDir().mkpath(m_directory);QImage image;const auto cached=m_directory+'/'+key+".jpg";
    if(!file.isEmpty())image.load(cached);
    if(image.isNull()&&!file.isEmpty()&&QFileInfo::exists(file.value("path").toString())){
        QTemporaryFile target(m_directory+'/'+key+"-XXXXXX.jpg");
        if(target.open()){const auto temporary=target.fileName();target.close();QProcess process;const double at=qMin(60.0,qMax(0.0,file.value("duration").toDouble()/10000.0));
            process.start("ffmpeg",{"-nostdin","-v","error","-y","-threads","1","-ss",QString::number(at,'f',2),"-i",file.value("path").toString(),"-map","0:v:0","-frames:v","1","-vf","scale=480:-2","-an","-sn",temporary});
            if(process.waitForStarted(2000)){if(!process.waitForFinished(8000)){process.kill();process.waitForFinished();}else if(process.exitCode()==0){image.load(temporary);if(!image.isNull()){QFile::remove(cached);QFile::rename(temporary,cached);}}else if(qEnvironmentVariableIsSet("ARCA_TEST_AUDIO_NULL"))qWarning()<<"Miniatura:"<<process.readAllStandardError();}
        }
    }
    if(image.isNull()){image=QImage(480,270,QImage::Format_RGB32);QPainter painter(&image);QLinearGradient gradient(0,0,480,270);gradient.setColorAt(0,QColor("#323D60"));gradient.setColorAt(1,QColor("#11151F"));painter.fillRect(image.rect(),gradient);painter.setPen(Qt::NoPen);painter.setBrush(QColor("#8B9CFF"));painter.drawPolygon(QPolygonF({QPointF(224,112),QPointF(224,158),QPointF(265,135)}));}
    if(size)*size=image.size();if(requested.width()>0&&requested.height()>0)return image.scaled(requested,Qt::KeepAspectRatio,Qt::SmoothTransformation);return image;
}
