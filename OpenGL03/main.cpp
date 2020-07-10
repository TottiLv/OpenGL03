//
//  main.cpp
//  OpenGL03
//
//  Created by lvjianxiong on 2020/7/10.
//  Copyright © 2020 lvjianxiong. All rights reserved.
//

#include "GLShaderManager.h"
#include "GLTools.h"
#include <GLUT/GLUT.h>

//矩形工具类
//1、利用GLMatrixStack加载单元矩阵、矩阵、矩阵相乘、压栈、出栈、缩放、平移、旋转
#include "GLMatrixStack.h"
//2、表示位置，通过设置vOrigin、vForward、vUp
#include "GLFrame.h"
//3、用来快速设置正/透视投影矩阵，完成坐标从2D->3D的映射过程
#include "GLFrustum.h"

//三角形批次类，帮助类，用来传输顶点、光照、纹理、颜色数据到存储着色器
#include "GLBatch.h"
//变换管道类，用来快速在代码中传输视图矩阵、投影矩阵、视图投影矩阵等
#include "GLGeometryTransform.h"

//存储着色器管理工具类
GLShaderManager shaderManager;

//模型视图矩阵
GLMatrixStack modelViewMatrix;
//投影矩阵
GLMatrixStack projectionMatrix;

//设置观察者视图坐标
GLFrame cameraFrame;
//设置图形环绕时，视图坐标坐标
GLFrame objectFrame;

//投影设置--图元绘制时的投影方式
GLFrustum viewFrustum;

//容器类（7中不同的图元对应7种容器对象）
GLBatch pointBatch;//点
GLBatch lineBatch;//线
GLBatch lineStripBatch;//线段
GLBatch lineLoopBatch;//线环
GLBatch triangleBatch;//金字塔
GLBatch triangleStripBatch;//六边形
GLBatch triangleFanBatch;//圆柱

//几何变换的管道
GLGeometryTransform transformPipeline;

//定义两种颜色
GLfloat vGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};
GLfloat vBlack[] = {0.0f, 0.0f, 0.0f, 1.0f};

//跟踪效果步骤--渲染不同的图形
int nStep = 0;

void DrawWireFrameBatch(GLBatch *pBatch){
    //1、填充图形内容
    
    shaderManager.UseStockShader(GLT_SHADER_FLAT, transformPipeline.GetModelViewProjectionMatrix(), vGreen);
    pBatch->Draw();
    
    //2、绘制边框部分
//    多边形偏移：在同一位置要绘制填充和边线，会产生z冲突，所以要偏移
    glPolygonOffset(-1.0f, -1.0f);
    //启用线的深度偏移
    glEnable(GL_POLYGON_OFFSET_LINE);
    //画反锯齿：让黑边好看些
    glEnable(GL_LINE_SMOOTH);
//    颜色混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
//    绘制边框
    //绘制线框几何黑色版 三种模式，实心，边框，点，可以作用在正面，背面，或者两面
    //通过调用glPolygonMode将多边形正面或者背面设为线框模式，实现线框渲染
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //设置线条宽度
    glLineWidth(2.5f);
    
    shaderManager.UseStockShader(GLT_SHADER_FLAT, transformPipeline.GetModelViewProjectionMatrix(), vBlack);
    pBatch->Draw();
    
    //3、将设置的属性还原
    //通过调用glPolygonMode将多边形正面或者背面设为全部填充模式
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}

/*
 通过glutReShaperFunc 注册为重塑函数
 当屏幕大小发生变化/或者第一次创建窗口时，会调用该函数调整窗口大小/视口大小
 */
void ChangeSize(int w, int h){
    //设置视口
    glViewport(0, 0, w, h);
    //设置投影矩阵
    viewFrustum.SetPerspective(35.0, float(w)/float(h), 1.0f, 500.0f);
    //加载投影矩阵 （获得投影矩阵，存储在矩阵中）
    projectionMatrix.LoadMatrix(viewFrustum.GetProjectionMatrix());
    //加载单元矩阵
    modelViewMatrix.LoadIdentity();
    //设置变换管道中模型矩阵和投影矩阵
    transformPipeline.SetMatrixStacks(modelViewMatrix, projectionMatrix);
}

/*
 通过glutDisplayFunc 注册为显示渲染函数
 当屏幕发生变化/或者开发者主动渲染会调用此函数，用来实现数据->渲染过程
 */
void RenderScene(void){
    //清空颜色、深度缓存区
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    //压栈 (栈作用：记录状态，进行撤回操作)
    modelViewMatrix.PushMatrix();
    //观察者矩阵
    //cameraFrame不是矩阵，将cameraFrame 构建成 观察者矩阵
    M3DMatrix44f mCamera;
    //执行后，mCamera中放入一个观察者矩阵
    cameraFrame.GetCameraMatrix(mCamera);
    //栈顶单元矩阵和mCamera相乘 的到一个新的Camera
    modelViewMatrix.MultMatrix(mCamera);
    
    //物体矩阵
    //利用mObjectFrame 构建成物体矩阵
    M3DMatrix44f mObjectFrame;
    //执行后，mObjectFrame中放入一个物体矩阵
    //只要使用 GetMatrix 函数就可以获取矩阵堆栈顶部的值，这个函数可以进行2次重载。
    //用来使用GLShaderManager 的使用。或者是获取顶部矩阵的顶点副本数据
    objectFrame.GetMatrix(mObjectFrame);
    
    //矩阵相乘 矩阵乘以矩阵堆栈的顶部矩阵，相乘的结果随后简存储在堆栈的顶部
    modelViewMatrix.MultMatrix(mObjectFrame);
    //    固定管线渲染点/线/线段/线环
    
    //模型视图矩阵
    shaderManager.UseStockShader(GLT_SHADER_FLAT, transformPipeline.GetModelViewProjectionMatrix(),vBlack);
    switch (nStep) {
        case 0:
            //设置点的大小
            glPointSize(4.0f);
            pointBatch.Draw();
            glPointSize(1.0f);
            break;
        case 1:
            //设置线的宽度
            glLineWidth(2.0f);
            lineBatch.Draw();
            glLineWidth(1.0f);
            break;
        case 2:
            //设置线的宽度
            glLineWidth(2.0f);
            lineStripBatch.Draw();
            glLineWidth(1.0f);
            break;
        case 3:
            //设置线的宽度
            glLineWidth(2.0f);
            lineLoopBatch.Draw();
            glLineWidth(1.0f);
            break;
        case 4:
            DrawWireFrameBatch(&triangleBatch);
            break;
        case 5:
            DrawWireFrameBatch(&triangleFanBatch);
            break;
        case 6:
            DrawWireFrameBatch(&triangleStripBatch);
            break;
    }
    
    //3、绘制完毕则还原矩阵
    modelViewMatrix.PopMatrix();
    //交互缓存区
    glutSwapBuffers();
}

/*
 自定义函数（调用一次，来做OpenGL初始化）
 设置需要渲染图形的相关顶点数据/颜色数据等数据装备工作
 */
void setUpRC(){
    //清除当前屏幕缓存区
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    //固定着色器初始化
    shaderManager.InitializeStockShaders();
    //平面着色器，变换管道一般在ChangeSize中进行初始化
    
    //观察者
    /*
     可设置三种模式
     MoveForward
     MoveRight
     MoveUp
     */
    cameraFrame.MoveForward(-15.0f);//观察者不动、移动物体
    //objectFrame.MoveForward(15.0f);//物体不同，观察者动
    //设置顶点数据(物体坐标系(基于物体本身（显示需要将物体坐标系转换为规范坐标系）))
    GLfloat vCoast[9] = {
        3, 3, 0,
        0, 3, 0,
        3, 0, 0
    };
    //提交批次类
    
    //点
    pointBatch.Begin(GL_POINTS, 3);
    pointBatch.CopyVertexData3f(vCoast);
    pointBatch.End();
    
    //线
    lineBatch.Begin(GL_LINES, 3);
    lineBatch.CopyVertexData3f(vCoast);
    lineBatch.End();
        
    //线段）
    lineStripBatch.Begin(GL_LINE_STRIP, 3);
    lineStripBatch.CopyVertexData3f(vCoast);
    lineStripBatch.End();
        
    //线环
    lineLoopBatch.Begin(GL_LINE_LOOP, 3);
    lineLoopBatch.CopyVertexData3f(vCoast);
    lineLoopBatch.End();
        
    //3、绘制金字塔准备
    //    定义金字塔顶点数据
    //    利用三角形批次类，使用GL_TRIANGLES绘制
        //通过三角形创建金字塔
    GLfloat vPyramid[12][3] = {
        -2.0f, 0.0f, -2.0f,
        2.0f, 0.0f, -2.0f,
        0.0f, 4.0f, 0.0f,
            
        2.0f, 0.0f, -2.0f,
        2.0f, 0.0f, 2.0f,
        0.0f, 4.0f, 0.0f,
            
        2.0f, 0.0f, 2.0f,
        -2.0f, 0.0f, 2.0f,
        0.0f, 4.0f, 0.0f,
            
        -2.0f, 0.0f, 2.0f,
        -2.0f, 0.0f, -2.0f,
        0.0f, 4.0f, 0.0f
    };
        
    //每3个顶点绘制一个新的三角形
    triangleBatch.Begin(GL_TRIANGLES, 12);
    triangleBatch.CopyVertexData3f(vPyramid);
    triangleBatch.End();
        
        
        
    //4、绘制六角形准备
    //    循环定义顶点数据
    //    利用三角形批次类，使用GL_TRIANGLE_FAN传输数据
    GLfloat vPoints[100][3];//100个顶点，每个顶点都是xyz
    int nVerts = 0;//记录当前是第几个顶点
    //半径
    GLfloat r = 3.0f;
    //原点 （x，y，z） = （0，0，0）
    vPoints[nVerts][0] = 0.0f;
    vPoints[nVerts][1] = 0.0f;
    vPoints[nVerts][2] = 0.0f;
        
    //M3D_2PI 就是2Pi 的意思，就一个圆的意思。 绘制圆形, M3D_2PI / 6.0f 圆形角度6等分
    for (GLfloat angle = 0; angle < M3D_2PI; angle += M3D_2PI / 6.0f) {
        //数组下标自增
        nVerts++;
        //弧长=半径*角度
        //根据cos，可求 角度 = arccos
        //x点坐标 cos(angle) * 半径
        vPoints[nVerts][0] = float(cos(angle)) * r;
        //y点坐标 sin(angle) * 半径
        vPoints[nVerts][1] = float(sin(angle)) * r;
        //z点的坐标
        vPoints[nVerts][2] = -0.5f;
    }
    // 结束扇形 前面一共绘制7个顶点（包括圆心）
    //添加闭合的终点
    nVerts++;
    vPoints[nVerts][0] = r;
    vPoints[nVerts][1] = 0;
    vPoints[nVerts][2] = 0.0f;
        
    //加载
    //GL_TRIANGLE_FAN 以一个圆心为中心呈扇形排列，共用相邻顶点的一组三角形
    triangleFanBatch.Begin(GL_TRIANGLE_FAN, 8);
    triangleFanBatch.CopyVertexData3f(vPoints);
    triangleFanBatch.End();
    
        
    //5、绘制三角形环准备
    //    循环定义顶点数据
    //    利用三角形批次类，使用GL_TRIANGLE_STRIP传输数据
    //三角形条带，一个小环或圆柱段
    //顶点下标
    int iCounter = 0;
    //半径
    GLfloat radius = 3.0f;
    //从0度~360度，以0.3弧度为步长
    for (GLfloat angle = 0.0f; angle <= 2.0*M3D_PI; angle += 0.3f) {
        //圆形顶点的x，y
        GLfloat x = radius * sin(angle);
        GLfloat y = radius * cos(angle);
            
        //绘制两个三角形（他们的x,y顶点一样，只是z点不一样）
        vPoints[iCounter][0] = x;
        vPoints[iCounter][1] = y;
        vPoints[iCounter][2] = -0.5;
        iCounter++;
            
        vPoints[iCounter][0] = x;
        vPoints[iCounter][1] = y;
        vPoints[iCounter][2] = 0.5;
        iCounter++;
    }
        
    //关闭循环：结束循环，在循环位置生成2个三角形
    vPoints[iCounter][0] = vPoints[0][0];
    vPoints[iCounter][1] = vPoints[0][1];
    vPoints[iCounter][2] = -0.5;
    iCounter++;
        
    vPoints[iCounter][0] = vPoints[1][0];;
    vPoints[iCounter][1] = vPoints[1][1];
    vPoints[iCounter][2] = 0.5;
    iCounter++;
        
    // GL_TRIANGLE_STRIP 共用一个条带（strip）上的顶点的一组三角形
    triangleStripBatch.Begin(GL_TRIANGLE_STRIP, iCounter);
    triangleStripBatch.CopyVertexData3f(vPoints);
    triangleStripBatch.End();
    
    //接下来进入到绘制 RenderScene
}

/*
 通过glutSpecialFunc 注册为特殊键盘处理函数
 */
void SpecialKeys(int key, int x, int y){
    if (key == GLUT_KEY_UP) {
        //围绕一个指定的xyz旋转--x
        objectFrame.RotateWorld(m3dDegToRad(-5.0f), 1.0f, 0.0f, 0.0f);
    }
    
    if (key == GLUT_KEY_DOWN) {
        //围绕一个指定的xyz旋转--x
        objectFrame.RotateWorld(m3dDegToRad(5.0f), 1.0f, 0.0f, 0.0f);
    }
    
    if (key == GLUT_KEY_LEFT) {
        //围绕一个指定的xyz旋转--y
        objectFrame.RotateWorld(m3dDegToRad(-5.0f), 0.0f, 1.0f, 0.0f);
    }
    
    if (key == GLUT_KEY_RIGHT) {
        //围绕一个指定的xyz旋转--y
        objectFrame.RotateWorld(m3dDegToRad(5.0f), 0.0f, 1.0f, 0.0f);
    }
    //重绘
    glutPostRedisplay();
}

/*
 根据空格次数，切换不同的窗口名称
 通过glutKeyboardFunc 注册为特殊键盘处理函数
 */
void KeyPressFunc(unsigned char key, int x, int y){
    if (key == 32) {
        nStep++;
        if (nStep > 6) {
            nStep = 0;
        }
    }
    
    switch (nStep) {
        case 0:
            glutSetWindowTitle("GL_POINTS");
            break;
        case 1:
            glutSetWindowTitle("GL_LINES");
            break;
        case 2:
            glutSetWindowTitle("GL_LINE_STRIP");
            break;
        case 3:
            glutSetWindowTitle("GL_LINE_LOOP");
            break;
        case 4:
            glutSetWindowTitle("GL_TRIANGLES");
            break;
        case 5:
            glutSetWindowTitle("GL_TRIANGLE_STRIP");
            break;
        case 6:
            glutSetWindowTitle("GL_TRIANGLE_FAN");
            break;
    }
    
    //手动触发重新渲染
    glutPostRedisplay();
}

int main(int argc,char* argv[]){
    //设置当前工作区域
    gltSetWorkingDirectory(argv[0]);
    //初始化GLUT库，这个函数只是传输命令参数并且初始化glut库
    glutInit(&argc, argv);
    //初始化双缓存窗口
    glutInitDisplayMode(GLUT_DOUBLE| GLUT_RGBA | GLUT_DEPTH | GLUT_STENCIL);
    //初始化GLUT窗口大小
    glutInitWindowSize(800, 600);
    glutCreateWindow("Shape");
    /*
     注册GLUT回调方法,GLUT内部运行一个本地消息循环，拦截适当的消息
     */
    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    glutSpecialFunc(SpecialKeys);
    glutKeyboardFunc(KeyPressFunc);
    
    /*
    初始化一个GLEW库，确保OpenGL API对程序完全可用
    在试图做任何渲染之前，要检查确定驱动程序的初始化过程中没有任何问题
    */
    GLenum glEnum = glewInit();
    if (glEnum != GLEW_OK) {
        fprintf(stderr,"glew error:%s\n",glewGetErrorString(glEnum));
        return 1;
    }
    
    //设置渲染环境
    setUpRC();
    
    glutMainLoop();
    return 0;
}
