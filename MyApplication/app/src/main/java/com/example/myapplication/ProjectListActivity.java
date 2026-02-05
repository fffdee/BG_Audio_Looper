package com.example.myapplication;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;
import java.util.List;

public class ProjectListActivity extends AppCompatActivity {
    private ListView lvProjects;
    private List<ImageProject> projectList;
    private List<String> projectNames;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_project_list);
        lvProjects = findViewById(R.id.lv_projects);
        TextView tvTitle = findViewById(R.id.tv_title);

        // 获取所有项目
        projectList = ProjectManager.getProjects(this);
        projectNames = new ArrayList<>();
        for (ImageProject project : projectList) {
            projectNames.add(project.getProjectName());
        }

        ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                android.R.layout.simple_list_item_1, projectNames);
        lvProjects.setAdapter(adapter);

        // 点击项目可查看详情或播放图片
        lvProjects.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                ImageProject project = projectList.get(position);
                Intent intent = new Intent(ProjectListActivity.this, ProjectDetailActivity.class);
                intent.putExtra("PROJECT_IMAGE_PATH", project.getMergedImagePath());
                startActivity(intent);
            }
        });
    }
}
